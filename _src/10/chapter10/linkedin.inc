<?php
// $Id: linkedin.inc,v 1.1.2.4 2010/05/13 12:29:36 bellesmanieres Exp $

/*
 * @file : LinkedIn API requests related functions
 */

/*
 * Called from submit callback at linkedin/token/%user
 * Let the user authorize us to gain acces to his profile, and store credentials.
 */
function linkedin_access_token($account) {

  //setting up variables
  $base_url = "https://api.linkedin.com/uas/oauth";
  
  $consumer_key = variable_get('linkedin_consumer_key', '');
  $consumer_secret = variable_get('linkedin_consumer_secret', '');
  $consumer = new OAuthConsumer($consumer_key, $consumer_secret, NULL);
  $signature = new OAuthSignatureMethod_HMAC_SHA1();
  $random = md5(rand());
  $callback = url('linkedin/token/'. $account, array('absolute' => TRUE)) .'?action='. $random ; //will be used to discard direct call to the path

  // First or renewal request
  if (!isset($_GET['action']) || $_GET['action'] != $_SESSION['random']) {
    $_SESSION['random'] = $random;
    $url = $base_url . "/requestToken";
    $request = OAuthRequest::from_consumer_and_token($consumer, NULL, 'POST', $url);
    $request->set_parameter("oauth_callback", $callback);
    $request->sign_request($signature, $consumer, NULL);
    $header = $request->to_header();    
    $response = _linkedin_http_request($url, $header, 'token_request');
    parse_str($response, $oauth);        
    if($oauth['oauth_problem']){
       drupal_set_message(t('There was a problem with the configuration of LinkedIn on this website. Please try again later.'), 'error');
      watchdog('linkedin', t('Linkedin reported the following response : @error'), array('@error' => $oauth['oauth_problem']), WATCHDOG_ERROR);
      drupal_goto('user');
      return;
    }
    if ($oauth['oauth_token']) {
      //store request token for next step
      db_query("DELETE FROM {linkedin_token} WHERE uid = %d", $account);
      $sql = array(
              'uid'           => $account,
              'token_key'     => $oauth['oauth_token'],
              'token_secret'  => $oauth['oauth_token_secret'],
              'type'          => 'request',
      );
      drupal_write_record('linkedin_token', $sql);

  
      //Redirect the user to the authentication/authorisation page. This will authorise the token in LinkedIn
      drupal_goto($base_url . '/authorize?oauth_token=' . $oauth['oauth_token']);
    }
    else {
      drupal_set_message(t('There was a problem with the configuration of LinkedIn on this website. Please try again later.'), 'error');
      watchdog('linkedin', t('The website could not communicate with LinkedIn. It is likely your API credentials are misconfigured.'), array(), WATCHDOG_ERROR);
      drupal_goto('user');
    }
    


  } else {
    //this is called when the OAuth callback is invoked. User has authorised the token.
    //Now retrieve the stored request token and ask for an access token

    $url = $base_url . '/accessToken';
    $result = db_result(db_query("SELECT token_secret FROM {linkedin_token} WHERE uid = %d AND type = 'request'", $account));
    $token = new OAuthConsumer($_REQUEST['oauth_token'], $result, 1);
    $request = OAuthRequest::from_consumer_and_token($consumer, $token, "POST", $url);
    $request->set_parameter("oauth_verifier", $_REQUEST['oauth_verifier']);
    $request->sign_request($signature, $consumer, $token);
    $header = $request->to_header();
    $response = _linkedin_http_request($url, $header, 'token_request');
    parse_str($response, $oauth);

  //save acces token for future requests
    $sql = array(
            'uid'           => $account,
            'token_key'     => $oauth['oauth_token'],
            'token_secret'  => $oauth['oauth_token_secret'],
            'type'          => 'access',
    );
    drupal_write_record('linkedin_token', $sql, array('uid'));
    drupal_goto("user/$account/edit/linkedin");
  }
}
/*
 * Main request function. Let us retrieve profile fields.
 * Return an array contening the fields requested.
 * Usage :
 * $account : the uid of the user we want to access infos
 * $fields : the fields we want to retrieve, as an array (see http://developer.linkedin.com/docs/DOC-1061 for complete list).
 * $flat : wether the returned array will preserve the hierarchy of infos of 'flatten' all fields to the same level.
 */
function linkedin_get_profile_fields($account, $fields = NULL, $flat = NULL) {
  $base_url = "https://api.linkedin.com/v1/people/~";
  if(!$fields) {
    $url = $base_url;
  } else {
    $append = '';
    foreach($fields as $field) {
      $append .= $field.',' ;
    }
    $url = $base_url .':('. trim($append, ',') .')' ;
  }
  
  $consumer_key = variable_get('linkedin_consumer_key', '');
  $consumer_secret = variable_get('linkedin_consumer_secret', '');
  $consumer = new OAuthConsumer($consumer_key, $consumer_secret, NULL);
  $signature = new OAuthSignatureMethod_HMAC_SHA1();
  $row = db_fetch_array(db_query("SELECT * FROM {linkedin_token} WHERE uid = %d AND type = 'access'", $account));
  if(!$row) {
    $response = 'refused';
    return $response;
  }
  $token = new OAuthConsumer($row['token_key'], $row['token_secret'], 1);
  $request = OAuthRequest::from_consumer_and_token($consumer, $token, "GET", $url);
  $request->sign_request($signature, $consumer, $token);
  $header = $request->to_header("https://api.linkedin.com");
  $response =  _linkedin_http_request($url, $header);
  parse_str($response);  
  if(!strpos($response, '</error-code>') === FALSE) {
    $response = 'refused';
  } else {
  $response = _linkedin_parse_fields($response, $flat);
  }
   return $response;
}

/*
 * Let us 'put' data into user profile.
 * Usage :
 * $account : the uid of the user we want to access infos
 * $field : the fields we want to update. At now, only 'current-status' is available.
 * $body : The content to be sent.
 */
function linkedin_put_profile_field($account, $field, $body) {
  $base_url = 'https://api.linkedin.com/v1/people/~/';
  $url = $base_url . $field ;
  $xml = '<'. $field .'>' . htmlspecialchars($body, ENT_NOQUOTES, "UTF-8") . '</'. $field .'>';
  $consumer_key = variable_get('linkedin_consumer_key', '');
  $consumer_secret = variable_get('linkedin_consumer_secret', '');
  $consumer = new OAuthConsumer($consumer_key, $consumer_secret, NULL);
  $signature = new OAuthSignatureMethod_HMAC_SHA1();
  $row = db_fetch_array(db_query("SELECT * FROM {linkedin_token} WHERE uid = %d AND type = 'access'", $account));
  $token = new OAuthConsumer($row['token_key'], $row['token_secret'], 1);
  $request = OAuthRequest::from_consumer_and_token($consumer, $token, 'PUT', $url, array());
  $request->sign_request($signature, $consumer, $token);
  $header = $request->to_header();
  $response =  _linkedin_http_request($url, $header, $xml);
  return $response;
}


/*
 * Some internal helper functions...
 */
function _linkedin_http_request($url, $header, $body = NULL) {

  $ch = curl_init();

  curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
  curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 0);
  curl_setopt($ch, CURLOPT_HTTPHEADER, array($header));
  curl_setopt($ch, CURLOPT_URL, $url);

  if ($body) {
    curl_setopt($ch, CURLOPT_POST, 1);
    if($body == 'token_request') {
      curl_setopt($ch, CURLOPT_POSTFIELDS, '');
    } else {
      curl_setopt($ch, CURLOPT_POSTFIELDS, $body);
      curl_setopt($ch, CURLOPT_HTTPHEADER, array($header, 'Content-Type: text/xml;charset=utf-8'));
      curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'PUT');
    }

  }

  $output = curl_exec($ch);
  curl_close($ch);
  return $output;
}

function _linkedin_parse_fields($data, $flat = FALSE) {

  $p = drupal_xml_parser_create($data);
  xml_parse_into_struct($p, $data, $vals, $index);
  xml_parser_free($p);
  $return = array();
  if($flat) {
     foreach($vals as $val) {
       if($val['type'] == "complete") {
        $tag = strtolower($val['tag']);
         $return[$tag] = $val['value'];
       }
     }
  } else {
  $pb = array();
  foreach($vals as $val) {
    if($val['type'] == "open") {
      $tag = strtolower($val['tag']);            
      array_push($pb, $tag);      
    } elseif($val['type'] == "close") {
      array_pop($pb);      
    } elseif($val['type'] == "complete") {
      $tag = strtolower($val['tag']);
      array_push($pb, $tag);
      _linkedin_iterate_xml_parsed($return, $pb, $val['value']);      
      array_pop($pb);
      
    }
  }
  }
  return $return;

  }



function _linkedin_iterate_xml_parsed(&$array, $pb, $val) {
  if ($pb) {
    $k = array_shift($pb);
    _linkedin_iterate_xml_parsed($array[$k], $pb, $val);
    return $array;
  } else {
    $array = $val;
  }
}
  
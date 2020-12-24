<?php
$provider = new stdClass;
$provider->disabled = FALSE; /* Edit this to true to make a default provider disabled initially */
$provider->name = 'twitter';
$provider->title = 'Twitter';
$provider->url = 'http://api.twitter.com';
$provider->consumer_advanced = array(
  'signature method' => 'HMAC-SHA1',
  'request token endpoint' => '/oauth/request_token',
  'authorization endpoint' => '/oauth/authenticate',
  'access token endpoint' => '/oauth/access_token',
);
$provider->mapping = array(
  'fields' => array(
    'uid' => array(
      'resource' => 'http://api.twitter.com/1/account/verify_credentials.json',
      'method post' => 0,
      'field' => 'id',
    ),
    'real name' => array(
      'resource' => 'http://api.twitter.com/1/account/verify_credentials.json',
      'method post' => 0,
      'field' => 'name',
    ),
    'avatar' => array(
      'resource' => 'http://api.twitter.com/1/account/verify_credentials.json',
      'method post' => 0,
      'field' => 'profile_image_url',
    ),
  ),
  'format' => 'json',
);
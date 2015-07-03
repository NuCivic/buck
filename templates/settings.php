<?php

$databases = array (
  'default' =>
  array (
    'default' =>
    array (
      'database' => '{{site_record.database}}',
      'username' => '{{site_record.username}}',
      'password' => '{{site_record.db_pass}}',
      'host' => 'localhost',
      'port' => '',
      'driver' => 'mysql',
      'prefix' => '',
    ),
  ),
);
$update_free_access = FALSE;
$base_url = 'http://{{site_record.uri}}';
ini_set('session.gc_probability', 1);
ini_set('session.gc_divisor', 100);
ini_set('session.gc_maxlifetime', 200000);
ini_set('session.cookie_lifetime', 2000000);
$conf['404_fast_paths_exclude'] = '/\/(?:styles)\//';
$conf['404_fast_paths'] = '/\.(?:txt|png|gif|jpe?g|css|js|ico|swf|flv|cgi|bat|pl|dll|exe|asp)$/i';
$conf['404_fast_html'] = '<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL \"@path\" was not found on this server.</p></body></html>';
drupal_fast_404();
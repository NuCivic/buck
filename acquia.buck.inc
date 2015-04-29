<?php


/**
 * SSH -t doesn't work with Acquia.
 */
function acquia_buck_sync_database($site_record) {
  buck_color_log('Syncing database from pantheon...', 'ok');
  $sync_source = drush_sitealias_get_record('@' . $site_record['sync-source']);

  buck_destroy_database($site_record);
  buck_create_database($site_record);

  $folder = $site_record['root'] . '/sites/default';
  // Add settings.php
  $str_config = BUCK_SETTINGS_PHP_TEMPLATE;
  $str_config = str_replace("#DATABASE#", $site_record['database'], $str_config);
  $str_config = str_replace("#USERNAME#", $site_record['dbuser'], $str_config);
  $str_config = str_replace("#PASSWORD#", $site_record['db_pass'], $str_config);
  $str_config = str_replace("#URL#", 'http://' . $site_record['uri'], $str_config);
  if (!is_readable($folder)) {
    drush_mkdir($folder);
  }

  $config_handle = fopen($folder . '/settings.local.php', "w+");
  fwrite($config_handle, $str_config);

  $temp_file = '/tmp/' . $site_record['database'] . '-' . time() . '.sql';
  $dump_cmd = 'drush @' . $site_record['sync-source']  . ' sql-dump > ' . $temp_file;
  $sql_cmd = 'mysql ' . $site_record['database'] . ' < ' . $temp_file;
  buck_color_log('Dumping database from acquia to ' . $temp_file  . ' ...', 'ok');
  buck_passthru_shell_exec($dump_cmd);
  buck_color_log('Importing database ...', 'ok');
  buck_passthru_shell_exec($sql_cmd);
  buck_color_log("... done", "ok");


}

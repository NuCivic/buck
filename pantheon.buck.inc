<?php

/**
 * @file
 * Specific settings for Pantheon.
 */

/**
 * Drush rsync doesn't work with Pantheon, using rsync instead.
 */
function pantheon_buck_sync_files($site_record) {
  drush_log('Syncing files from pantheon...', 'ok');

  $files_dir = $site_record['root'] . '/' . $site_record['path-aliases']['%files'];
  // Make sure we have something to rsync into.
  if (!file_exists($files_dir)) {
    drush_mkdir($files_dir);
  }
  $sync_source = drush_sitealias_get_record('@' . $site_record['sync-source']);
  $cmd = '/usr/bin/rsync -rlvz --ipv4 --progress -e \'ssh -p 2222\' ' . $sync_source['remote-user'] . '@' . $sync_source['remote-host'] . ':files/ ' . $site_record['root'] . '/' . $site_record['path-aliases']['%files'] . '/';
  buck_shell_exec($cmd);
  drush_log("... done", "ok");
}

/**
 * Drush sql-sync doesn't work with Pantheon.
 */
function pantheon_buck_sync_database($site_record) {
  drush_log('Syncing database from pantheon...', 'ok');
  $sync_source = drush_sitealias_get_record('@' . $site_record['sync-source']);

  if (!drush_sql_db_exists($site_record) || !file_exists($site_record['root'] . '/sites/' . $site_record['uri'] . '/settings.php')) {
    // Create database.
    $site_record['db_pass'] = drush_generate_password();
    $db_url = 'mysql://' . $site_record['database'] . ':' . $site_record['db_pass']  . '@localhost/' . $site_record['database'];

    $user = posix_getpwuid(posix_geteuid());
    $db_super = parse_ini_file($user['dir'] . '/.my.cnf');
    if (!$db_super['user']) {
      drush_set_error("Your .my.cnf file is not setup correctly.");
      return;
    }
    drush_invoke_process(
      '@self',
      'sql-create',
      array("@" . $site_record['sync-source'], "@" . $site_record['#name']),
      array('--db-url=' . $db_url, '--db-su=' . $db_super['user'], '--db-su-pw=' . $db_super['password']),
      array('interactive' => TRUE)
    );

    // Add settings.php
    $str_config = BUCK_SETTINGS_PHP_TEMPLATE;
    $str_config = str_replace("#DATABASE#", $site_record['database'], $str_config);
    $str_config = str_replace("#USERNAME#", $site_record['database'], $str_config);
    $str_config = str_replace("#PASSWORD#", $site_record['db_pass'], $str_config);
    $str_config = str_replace("#URL#", 'http://' . $site_record['uri'], $str_config);

    drush_mkdir($site_record['root'] . "/sites/" . $site_record['uri']);
    $config_handle = fopen($site_record['root'] . "/sites/" . $site_record['uri'] . "/settings.php", "w+");
    fwrite($config_handle, $str_config);
  }

  $tmp_dir = drush_tempdir();
  $temp_file = '/tmp/' . $site_record['database'] . '-' . time() . '.sql';
  $dump_cmd = 'drush @' . $site_record['sync-source']  . ' sql-dump > ' . $temp_file;
  $dump_cmd = 'mysqldump -u ' . $site_record['sync-sql-user'] . ' -p' . $site_record['sync-sql-pass'] . ' -h dbserver.' . $sync_source['remote-user'] . '.drush.in -P ' . $site_record['sync-sql-port']  . ' ' . $site_record['sync-sql-db'] . ' > ' . $temp_file;
  buck_shell_exec($dump_cmd);
  $db_opt = '';
  if ($site_record['db_pass']) {
    $db_url = 'mysql://' . $site_record['database'] . ':' . $site_record['db_pass']  . '@localhost/' . $site_record['database'];
    $db_opt = '  --db-url=' . $db_url;
  }
  $sql_cmd = 'drush ' . '@' . $site_record['#name'] . ' sqlc < ' . $temp_file . $db_opt;
  buck_shell_exec($sql_cmd);
  drush_log("... done", "ok");
}

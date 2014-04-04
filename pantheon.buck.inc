<?php

/**
 * @file
 * Specific settings for Pantheon.
 */

/**
 * Drush rsync doesn't work with Pantheon, using rsync instead.
 */
function pantheon_buck_sync_files($site_record) {
  buck_color_log('Syncing files from pantheon...', 'ok');

  $files_dir = $site_record['root'] . '/' . $site_record['path-aliases']['%files'];
  // Make sure we have something to rsync into.
  if (!file_exists($files_dir)) {
    drush_mkdir($files_dir);
  }
  $sync_source = drush_sitealias_get_record('@' . $site_record['sync-source']);
  $cmd = '/usr/bin/rsync -av --ipv4 --progress -e \'ssh -p 2222\' ' . $sync_source['remote-user'] . '@' . $sync_source['remote-host'] . ':files/ ' . $site_record['root'] . '/' . $site_record['path-aliases']['%files'] . '/';
  buck_passthru_shell_exec($cmd);
  buck_color_log("... done", "ok");
}

/**
 * Drush sql-sync doesn't work with Pantheon.
 */
function pantheon_buck_sync_database($site_record) {
  buck_color_log('Syncing database from pantheon...', 'ok');
  $sync_source = drush_sitealias_get_record('@' . $site_record['sync-source']);

  $site_record['multisite'] = TRUE;
  buck_destroy_database($site_record);
  buck_create_database($site_record);
  drush_shell_exec('mysql -e "grant usage on *.* to ' . $site_record['database'] . '@localhost identified by ' . "'" . $site_record['db_pass'] . "'" . '"');
  drush_shell_exec('mysql -e "grant all privileges on ' . $site_record['database'] . '.* to ' . $site_record['database'] . '@localhost"');

  $temp_file = '/tmp/' . $site_record['database'] . '-' . time() . '.sql';
  $dump_cmd = 'drush @' . $site_record['sync-source']  . ' sql-dump > ' . $temp_file;
  $dump_cmd = 'mysqldump -u ' . $sync_source['sync-sql-user'] . ' -p' . $sync_source['sync-sql-pass'] . ' -h dbserver.' . $sync_source['remote-user'] . '.drush.in -P ' . $sync_source['sync-sql-port']  . ' ' . $sync_source['sync-sql-db'] . '  > ' . $temp_file;
  buck_passthru_shell_exec($dump_cmd);
  $db_opt = '';
  if ($site_record['db_pass']) {
    $db_url = 'mysql://' . $site_record['database'] . ':' . $site_record['db_pass']  . '@localhost/' . $site_record['database'];
    $db_opt = '  --db-url=' . $db_url;
  }
  else {
    $db_url = 'mysql://root:@localhost/' . $site_record['database'];
    $db_opt = '  --db-url=' . $db_url;
  }

  $sql_cmd = 'drush ' . '@' . $site_record['#name'] . ' sqlc < ' . $temp_file . $db_opt;
  buck_passthru_shell_exec($sql_cmd);
  buck_color_log("... done", "ok");
}

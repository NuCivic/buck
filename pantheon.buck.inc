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

  buck_create_database($site_record);

  $tmp_dir = drush_tempdir();
  $temp_file = '/tmp/' . $site_record['database'] . '-' . time() . '.sql';
  $dump_cmd = 'drush @' . $site_record['sync-source']  . ' sql-dump > ' . $temp_file;
  buck_shell_exec($dump_cmd);
  $db_opt = '';
  if ($site_record['db_pass']) {
    $db_url = 'mysql://' . $site_record['database'] . ':' . $site_record['db_pass']  . '@localhost/' . $site_record['database'];
    $db_opt = '  --db-url=' . $db_url;
  }
  $sql_cmd = 'drush ' . '@' . $site_record['#name'] . ' sqlc < ' . $temp_file . $db_opt;
  drush_print_r($sql_cmd);
  buck_shell_exec($sql_cmd);
  drush_log("... done", "ok");
}

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
  $cmd = '/usr/bin/rsync -av --ipv4 --progress -e \'ssh -p 2222\' ' . $site_record['remote-user'] . '@' . $site_record['remote-host'] . ':files/ ' . $site_record['root'] . '/' . $site_record['path-aliases']['%files'] . '/';
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

  $temp_file = '/tmp/' . $site_record['database'] . '-' . time() . '.sql';

  $dump_cmd = 'drush ' . $site_record['sync-source']  . ' sql-dump > ' . $temp_file;
  $sql_cmd = 'mysql ' . $site_record['database'] . ' < ' . $temp_file;
  buck_color_log('Dumping database from pantheon to ' . $tmp_file  . ' ...', 'ok');
  buck_passthru_shell_exec($dump_cmd);
  buck_color_log('Importing database ...', 'ok');
  buck_passthru_shell_exec($sql_cmd);
  buck_color_log("... done", "ok");
}

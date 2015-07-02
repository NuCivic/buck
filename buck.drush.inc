<?php

/**
 * @file
 *   Buck drush commands.
 */

 // My dumb loader.
foreach (glob(dirname(__FILE__) . "/*buck.inc") as $filename) {
  include_once $filename;
}

/**
 * Provides args for destroy command.
 */
function _buck_destroy_arg_list() {
  // The same as the create list.
  return _buck_create_arg_list();
}

/**
 * Provides args for perms command.
 */
function _buck_perms_arg_list() {
  return array(
    'root' => 'root',
  );
}

/**
 * Implements hook_drush_command()
 * @return array
 */
function buck_drush_command() {
  $items = array();
  $items['buck'] = array(
    'description' => "Fetch a site. If 'makefile' is present will build via  makefile. If 'sync-source' is present will sync local version to sync-source. If neither are present will download Drupal Core 7.x with specified root, uri, and other included options.",
    'options' => array(
      'uri'       => 'Required.  Domain name of new site.',
      'root'     => ' Required.  Path to drupal code."',
      'git' => 'Optional. Array consisting of "url" and "branch". Defaults to Drupal Core 7.x. Ie if you do not specify you will download Drupal Core.',
      'branch' => 'Optional. Overrides git branch',
      'makefile' => 'Optional. URL of make file.',
      'profile'      => 'Optional.  Name of profile to install.  Defaults "standard".',
      'dbuser'       => 'Optional. Defaults to "root"',
      'dbpassword'   => 'Optional. Root mysql password.  Defaults to ""',
      'makefile'     => 'Optional.  Path to drupal make file."',
      'db-url'       => 'Optional.  DB url.  Defaults to "mysql://[domain]]:[domain]@localhost/[domain]" (replace . with _ in [domain])',
      'db-name'      => 'Optoina.   DB name.',
      'db-prefix'    => 'Optional.  DB table prefix.  Defaults to ""',
      'account-name' => 'Optional.  Name of user 1.  Defaults to "admin"',
      'account-pass' => 'Optional.  Password user 1.  Defaults to "admin"',
      'account-mail' => 'Optional.  Email of user 1.  Defaults to "admin@[domain]"',
      'locale'       => 'Optional.  Installation profile.  Defaults to "en"',
      'site-name'    => 'Optional.  Site name.  Defaults to "[domain]"',
      'site-mail'    => 'Optional.  Site default email.  Defaults to "info@[domain]"',
      'sync-source' => 'Optional.  Site that @self will sync from."',
      'no-perms' => 'Skip permission check',
      'no-web' => 'Skip webserver creation or update',
    ),
    'examples' => array(
      'Create D7 site in one command: setup dns, apache, create db, download d6, run installer, open in firefox' => 'drush buck-create --domain=example1.dev',
      'Quick D6 site' => 'drush qc --domain=example2.dev --makefile=d6.make',
      'Just DNS (/etc/hosts)' => 'drush buck-create dns --domain=example.dev',
      'Just apache (/etc/apache2/sites-enabled/)' => 'drush buck-create apache --domain=example.dev --codepath=/path/to/drupal',
      'Just database (mysql)' => 'drush buck-create database --domain=example.dev',
      'Just code (drush make)' => 'drush buck-create code --domain=example.dev',
      'Just install (or quick reinstall)' => 'drush buck-create install --domain=example.dev',
    ),
    'aliases' => array('bf'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['buck-destroy'] = array(
    'description' => "Destroy a buck development website.",
    'aliases' => array('bd'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['buck-sql'] = array(
    'description' => "Sync a databse.",
    'aliases' => array('bsql'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'options' => array(
      'dbuser'       => 'Optional. Defaults to "root"',
      'dbpassword'   => 'Optional. Root mysql password.  Defaults to ""',
      'db-url'       => 'Optional.  DB url.  Defaults to "mysql://[domain]]:[domain]@localhost/[domain]" (replace . with _ in [domain])',
      'db-prefix'    => 'Optional.  DB table prefix.  Defaults to ""',
      'sync-source' => 'Optional.  Site that @self will sync from."',
    ),
  );
  $items['buck-perms'] = array(
    'description' => "Fix permissions for a buck development website:
    \$ sudo chown -R CURRENT_USER:APACHE_USER <codepath>
    \$ sudo chmod -R a=,u=rwX,g=rX <codepath>
    \$ sudo find <root>/sites -type d -name files -exec chmod -R a=,ug=rwX '{}' \;
    \$ sudo find <root>/sites -type d -name files_private -exec chmod -R a=,ug=rwX '{}' \;",
    'arguments' => array(
    ),
    'options' => array(
      'root' => 'Path to drupal code.  Defaults to ~/websites/[domain]',
      'user' => 'User',
      'group' => 'Group',
    ),
    'examples' => array(
      'Simplest format' => 'drush qf --domain=example.dev',
      'Use different codepath' => 'drush buck-perms --domain=example.dev --codepath=/path/to/drupal',
    ),
    'aliases' => array('bf'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  return $items;
}

/**
 * Drush callback function for 'drush buck'.
 *
 * This function goes step-by-step in creating or updating the new site.
 *
 * @return bool
 *   TRUE if successful.
 */
function drush_buck($site = NULL) {
  $site_record = drush_sitealias_get_record($site);

  try {
    $site_record = _buck_env_prepare($site_record);
    buck_create_dns($site_record);
    // This is a site install from a makefile. Site is installed through make.
    if (isset($site_record['makefile'])) {
      buck_profile_make($site_record);
      buck_profile_install($site_record);
      buck_revert_features($site_record);
    }
    // Site has an installation script. No code rebuild.
    elseif ($site_record['script']) {
      buck_create_root($site_record);
      buck_sync_git($site_record);
      buck_profile_install($site_record);
      buck_script_install($site_record);
    }
    // This is a normal site with non-local code, database, and media files.
    elseif ($site_record['sync-source']) {
      buck_create_root($site_record);
      buck_sync_git($site_record);
      buck_sync_files($site_record);
      buck_sync_database($site_record);
      buck_command_drush($site_record);
    }
    // This is a site we are installing from scratch. Requires a profile which
    // defaults to standard.
    else {
      buck_create_root($site_record);
      buck_sync_git($site_record);
      buck_profile_install($site_record);
    }
    // Add vhost.
    if (!drush_get_option('no-web', FALSE)) {
      buck_create_webserver_config($site_record);
    }

    // Cleanup
    if (!drush_get_option('no-perms', FALSE)) {
      buck_permissions_cleanup($site_record);
    }

  } catch (Exception $e) {
    $message = $e->getMessage();
    buck_color_log("Command failed: $message", "error");
    return false;
  }

  buck_color_log("Command completed successfully.", "success");

  return TRUE;
}

/**
 * Drush callbackfunction for 'drush buck-perms'.
 *
 * This function fixes permissions for a drupal site.
 *
 * @reurn true on success
 */
function drush_buck_perms($site = NULL) {
  $site_record = drush_sitealias_get_record($site);
  $site_record = _buck_env_prepare($site_record);

  $drupal_path = $site_record['root'];
  $files_dir = $drupal_path . '/' . $site_record['path-aliases']['%files'];
  $drupal_user = isset($site_record['user']) ? $site_record['user'] : exec('echo "$(whoami)"');
  $httpd_group = isset($site_record['group']) ? $site_record['group'] : _drush_get_os() == 'Darwin' ? '_www' : 'www-data';

  buck_color_log("Changing ownership of all contents of $drupal_path user => $drupal_user group => $httpd_group", 'ok');
  buck_shell_exec("sudo chown -R $drupal_user:$httpd_group $drupal_path");

  buck_color_log("Changing permissions of all directories inside $drupal_path to rwxr-x--- ...", 'ok');
  buck_shell_exec("find $drupal_path -type d -exec chmod u=rwx,g=rx,o= '{}' \;");

  buck_color_log("Changing permissions of all files inside $drupal_path to rw-r----- ...", 'ok');
  buck_shell_exec("find $drupal_path -type f -exec chmod u=rw,g=r,o= '{}' \;");

  buck_color_log("Changing permissions of \"files\" directories in $drupal_path/sites to rwxrwx--- ...", 'ok');
  buck_shell_exec("find $drupal_path/sites -type d -name files -exec chmod ug=rwx,o= '{}' \;");

  buck_color_log("Changing permissions of all files inside $files_dir to rw-rw---- ...", 'ok');
  buck_shell_exec("find $files_dir -type d -exec chmod ug=rwx,o= '{}' \;");

  // TODO: add support for private files.
  buck_color_log("Changing permissions of all directories inside $files_dir to rwxrwx--- ..", 'ok');
  buck_shell_exec("find $files_dir -type f -exec chmod ug=rw,o= '{}' \;");

  return TRUE;
}

/**
 * Drush callback function for 'drush buck-destroy'
 *
 * Works step-by-step to destroy the site.
 *
 * @return true on success
 */
function drush_buck_destroy($site_record = array()) {
  $site_record = drush_sitealias_get_record('@self');

  try {
    if (!$site_record = _buck_env_prepare($site_record)) {
      return;
    }
    buck_destroy_dns($site_record);
    buck_destroy_database($site_record);
    buck_destroy_webserver_config($site_record);
    buck_destroy_root($site_record);
  } catch (Exception $e) {
    $message = $e->getMessage();
    buck_color_log("Command failed: $message", "error");
    return false;
  }

  buck_color_log("Command completed successfully.", "success");
  return true;
}

/**
 * Drush callback function for 'drush buck-destroy'
 *
 * Works step-by-step to destroy the site.
 *
 * @return true on success
 */
function drush_buck_sql($site_record = array()) {
  $site_record = drush_sitealias_get_record('@self');

  try {
    if (!$site_record = _buck_env_prepare($site_record)) {
      return;
    }
    buck_sync_database($site_record);
    buck_command_drush($site_record);
  } catch (Exception $e) {
    $message = $e->getMessage();
    buck_color_log("Command failed: $message", "error");
    return false;
  }

  buck_color_log("Command completed successfully.", "success");
  return true;
}

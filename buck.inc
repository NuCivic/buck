<?php

/**
 * @file
 * This is an include file.  These are the "callback" functions that drush
 * uses to get the job done.  See buck.drush.inc for the definition of
 * the commands.
 */


// It is often optimal to have a /var/www/BASE_ROOT/WEBROOT so that you can
// add things like private_files or logs to the BASE_ROOT where they are
// accessible to the admin user but not to the web.
define('BUCK_WEBROOT', 'webroot');

define('BUCK_BUILD_MAKE_TEMPLATE_FILE', 'git_build_make.php');
define('BUCK_APACHE_TEMPLATE_FILE', 'vhost.conf');
define('BUCK_SETTINGS_PHP_TEMPLATE_FILE', 'settings.php');

/**
 * Helper function for running SQL on a local mysql database.
 *
 * @param <type> $sql
 */
function buck_mysql_exec($sql, $dbuser, $dbpassword) {
  buck_shell_exec("echo \"$sql\" | mysql -u$dbuser -p$dbpassword");
}

/**
 * Helper function for dereferencing paths.
 *
 * @param <type> $pathin
 * @return <type>
 */
function buck_fixpath($pathin) {
  // fix ~'s
  $homedir = "/home/" . exec('whoami');
  $pathout = str_replace("~", $homedir, $pathin);
  // realpath fails on non-existant folders.  So it's hard to fix .'s and ..'s
  return $pathout;
}

/**
 * Create the DNS.  Edits /etc/hosts on local machine.
 * Entries in file are marked with #buck
 */
function buck_create_dns($site_record) {
  $domain = $site_record['uri'];
  $file = file_get_contents("/etc/hosts");
  // make sure to test for fully spaces parsed domain in hosts file.
  // otherwise shortened partial domains will throw an exception.
  $domainplus = " " . $site_record['uri'] . " ";
  if (!strpos($file, $domainplus)) {
    buck_color_log("Creating DNS config (add $domain to /etc/hosts) ...", 'ok');
    buck_shell_exec("echo '127.0.0.1 $domain #' | sudo /usr/bin/tee -a /etc/hosts");
    buck_color_log('... done.', 'ok');
  }
  else {
    buck_color_log("DNS config $domain already added to /etc/hosts", 'ok');
  }
}

/**
 * Destroy the DNS.  Edit /etc/hosts file on local machine
 */
function buck_destroy_dns($site_record) {
  $domain = $site_record['uri'];
  drush_print_r('sudo sed -i \"/ $domain .*#/d\" /etc/hosts');
  buck_shell_exec("sudo sed -i \"/ $domain .*#/d\" /etc/hosts");
}

/**
 * Configure apache virtualhost file using the template.
 */
function buck_create_webserver_config($site_record) {
  $domain = $site_record['uri'] . '.conf';
  if (file_exists("/etc/apache2/sites-available/$domain")) {
    if (drush_confirm(dt('Vhost ' .  $domain . ' already exists. Do you want to keep its contents?'))) {
      return;
    }
  }

  $twig = buck_create_twig_instance();
  $str_config = $twig->render(
    BUCK_APACHE_TEMPLATE_FILE,
    array(
      'path' => $site_record['root'],
      'domain' => $site_record['uri'],
    )
  );

  $config_handle = fopen("/etc/apache2/sites-available/$domain", "w+");
  fwrite($config_handle, $str_config);

  buck_shell_exec("sudo a2ensite $domain");

  // restart apache
  buck_shell_exec("sudo /usr/sbin/apachectl graceful");

  buck_color_log('... done.', 'ok');
}

/**
 * Destroy the apache virtual hosts config. a2dissite, then rm file.
 */
function buck_destroy_webserver_config($site_record) {
  $domain = $site_record['uri'];
  buck_shell_exec("sudo a2dissite $domain", false);
  $filename = "/etc/apache2/sites-available/$domain";
  if (file_exists($filename)) unlink($filename);  // unlink = delete file
  buck_color_log("Removing apache config (/etc/apache2/sites-enabled/$domain)", 'ok');
  buck_shell_exec("sudo apachectl graceful");
}

/**
 * Destroys the database using SQL.
 */
function buck_destroy_database($site_record) {
  buck_color_log('Destroying database ' . $site_record['database'] . ' ...', 'ok');
  // There is no sql-destroy in drush.
  drush_shell_exec('mysql -e "drop database ' . $site_record['database'] . '"');
}

/**
 * Prepares drush commands.
 */
function buck_command_drush($site_record) {
  if (isset($site_record['drush_command'])) {
    foreach ($site_record['drush_command'] as $command => $vars) {
      // Allow for multiple versions of the same command.
      if (!isset($vars['arg'])) {
        foreach ($vars as $subvars) {
          buck_fire_command($command, $subvars);
        }
      }
      else {
        buck_fire_command($command, $vars);
      }
    }
  }
}

/**
 * Fires drush commands.
 */
function buck_fire_command($command, $vars) {
  $option = isset($vars['option']) ? $vars['option'] : "";
  drush_print_r($command . ' ' . $vars['arg']);
  drush_invoke_process(
    '@self',
    $command . ' ' . $vars['arg'],
    array($option)
  );
}

/**
 * Reverts selected features for site.
 */
function buck_revert_features($site_record) {
  if ($site_record['features_revert']) {
    if (is_array($site_record['features_revert'])) {
      $cmd = 'features-revert ' . trim(implode(' ',$site_record['features_revert']));
    }
    else {
      $cmd = 'features-revert-all';
    }
    drush_invoke_process(
      '@self',
      $cmd
    );
  }
}

/**
 * Creates a base directory.
 *
 * If there is already one installed at 'base_root' then it prompts the user
 * if they want to create a backup.
 */
function buck_create_root($site_record) {
  $codepath = $site_record['base_root'];
  $tar_file = $site_record['database'] . '-' . date('y-m-d-h') . '.tar.gz';
  $tar_dir = '/var/www/' . trim(strrchr($site_record['base_root'], '/'), '/');

  // Create or make sure webroot installed.
  if (file_exists($codepath)) {
    if (drush_confirm(dt('Folder ' . $site_record['base_root'] . ' already exists. Do you want to keep it? "No" will create archive.'))) {
      return;
    }
    buck_color_log("Archiving existing directory $codepath to $tar_file", "ok");
    buck_shell_exec("tar zcfP $tar_file $codepath");
    buck_color_log("Removing existing directory $codepath", "ok");
    drush_delete_dir("$codepath");
    buck_color_log("... done", "ok");
    buck_color_log("Creating directory $codepath", "ok");
    drush_mkdir($site_record['base_root']);
    buck_color_log("... done", "ok");
  }
  else {
    buck_color_log("Creating directory $codepath", "ok");
    drush_mkdir($site_record['base_root']);
    buck_color_log("... done", "ok");
  }
}

/**
 * Removes code, media and other files.
 */
function buck_destroy_root($site_record) {
  $root = $site_record['base_root'];
  buck_color_log("Removing existing directory $root", "ok");
  drush_shell_exec("sudo rm -rf $root");
  buck_color_log("... done", "ok");
}

/**
 * Updates git code.
 */
function buck_git_pull($directory) {
  // TODO: Check to make sure that there are no
  // uncommitted changes etc.
  $command = 'git pull --rebase';
  buck_passthru_shell_cd_and_exec($directory, 'git pull --rebase');
}

/**
 * Updates existing profile managed by git.
 */
function buck_update_profile($site_record) {
  buck_color_log("Updating profile", "ok");
  // Drush make doesn't let you pass the root directory as an arg:
  // http://drupal.org/node/1673676.
  drush_shell_cd_and_exec($site_record['root'], "drush make --no-gitinfofile --contrib-destination=. --no-core --working-copy -y -v   " . $site_record['makefile']);
  buck_color_log("... done", "ok");
}

/**
 * This is my silly version of module_invoke_all() but instead it takes
 * over execution completely if it has a function to return.
 */
function buck_host_include($site_record, $function) {
  if (isset($site_record['sync-host']) && function_exists($site_record['sync-host'] . '_' . $function)) {
    call_user_func($site_record['sync-host'] . '_' . $function, $site_record);
    return TRUE;
  }
}

/**
 * Sync files from remote to local.
 */
function buck_sync_files($site_record) {
  if (buck_host_include($site_record, __FUNCTION__)) {
    return;
  }
  $files_dir = $site_record['root'] . '/' . $site_record['path-aliases']['%files'];
  // Make sure we have something to rsync into.
  if (!file_exists($files_dir)) {
    drush_mkdir($files_dir);
  }

  // See: http://drupal.org/node/1613376.
  $alias = drush_sitealias_get_record('@self');
  buck_clean_alias($alias);
  drush_invoke_process(
    $alias,
    'rsync',
    array("@" . $site_record['sync-source'] . ":%files", "@" . $site_record['#name'] . ":%files"),
    array('-y'),
    array('interactive' => TRUE)
  );
  buck_color_log("... done", "ok");
}

/**
 * Removes alias elements that are not removed by 'invoke' => TRUE.
 * See: http://drupal.org/node/1613376 for discussion. This is most likely a bug
 * in buck.
 */
function buck_clean_alias(&$alias) {
  $additional_args = buck_alias_args();
  foreach ($additional_args as $arg) {
    unset($alias[$arg]);
  }
}

/**
 * List of args that are introduced in the site-alias.
 */
function buck_alias_args() {
  return array(
    'sync-source',
    'sync-host',
    'db-su',
    'site-name',
    'remote-sql-type',
    'remote-file-type',
  );
}

/**
 * Clone or pull a git repo.
 */
function buck_sync_git($site_record) {
  $root = isset($site_record['base_root']) ? $site_record['base_root'] : $site_record['root'];
  buck_git_update($root, $site_record['git']);
}

/**
 * Updates a git repo or clones if it doesn't exist.
 */
function buck_git_update($git_dir, $git_args = NULL) {
  if (file_exists($git_dir . '/.git')) {
    buck_color_log('Git repo at ' . $git_dir . '/.git already exists. Updating...', "ok");
    buck_git_pull($git_dir);
    buck_color_log("... done", "ok");
    return ;
  }
  $branch = isset($git_args['branch']) ? '--branch ' . $git_args['branch'] : '';
  $command = 'git clone ' . $branch . ' ' . $git_args['url'] . ' ' . $git_dir;
  buck_color_log('Downloading code to ' . $git_dir . ' (takes a minute, check network activity) ...', 'ok');
  print_r($command);
  if (!system($command)) {
    return drush_set_error('DRUSH_PM_GIT_CHECKOUT_PROBLEMS', dt('Unable to clone project.'));
  }
  buck_color_log("... done", "ok");
}

/**
 * Installs site using script.
 */
function buck_script_install($site_record) {
  buck_color_log('Invoking ' . $site_record['script'] . ' ...', 'ok');

  drush_op('chdir', $site_record['base_root']);
  buck_passthru_shell_cd_and_exec($site_record['base_root'], '/bin/bash ' . $site_record['script']);
  buck_color_log("... done", "ok");
}
/**
 * Installs site using profile.
 */
function buck_profile_install($site_record) {
  buck_color_log('Installing ' . $site_record['profile'] . ' ...', 'ok');

  drush_op('chdir', $site_record['root']);
  $user = posix_getpwuid(posix_geteuid());
  if (!$site_record['db-su']) {
    $db_super = parse_ini_file($user['dir'] . '/.my.cnf');
    if (!$db_super['user']) {
      throw new Exception("Your .my.cnf file is not setup correctly.");
    }
    $site_record['db-su'] = $db_super['user'];
    $site_record['db-su-pw'] = $db_super['password'];
  }
  $opts = array(
    "--root=" . $site_record['root'],
    "-v",
    "--db-su=" . $site_record['db-su'],
    "--db-su-pw=" . $site_record['db-su-pw'],
    "--account-name=" . $site_record['account-name'],
    "--account-pass=" . $site_record['account-pass'],
    "--site-name=" . $site_record['site-name'],
    '--sites-subdir=default'
  );
  // Supply db credentials if we can't connect. Output string will be
  // empty spaces hence strlen.
  $opts[] = '--db-url=mysql://' . $site_record['db-su'] . ':' . $site_record['db-su-pw'] . '@localhost/' . $site_record['database'];
  drush_invoke_process(
    '@self',
    'site-install',
    array($site_record['profile']),
    $opts,
    array('#integrate' => TRUE)
  );
  buck_color_log("... done", "ok");
}

/**
 * Makes site via make file.
 */
function buck_profile_make($site_record) {
  if (isset($site_record['makefile'])) {
    buck_color_log("Downloading and installing through make file ...", 'ok');

    buck_destroy_code($site_record);
    buck_git_update($temp_dir, $site_record['git']);

    $temp_dir = drush_tempdir();
    $twig = buck_create_twig_instance();
    $str_config = $twig->render(
      BUCK_BUILD_MAKE_TEMPLATE_FILE,
      array(
        'site_record' => $site_record
      )
    );

    $config_handle = fopen($temp_dir . '/build-' . $site_record['profile'] . '.make', "w+");
    fwrite($config_handle, $str_config);
    buck_passthru_shell_cd_and_exec($temp_dir, 'drush make -y --no-cache --prepare-install --working-copy ' . $site_record['makefile'] . ' ' . $site_record['root']);

    buck_color_log('... done.', 'ok');
  }
}

/**
 * Syncs your database from a remote database.
 */
function buck_sync_database($site_record) {
  if (buck_host_include($site_record, __FUNCTION__)) {
    return;
  }
  if ($site_record['sql-dump']) {
    buck_color_log("Grabbing the database ...", 'ok');
    $tmp_file_name = '/tmp/' . $site_record['database'] . "-" . time() . ".sql";

    $sync_source = drush_sitealias_get_record('@' . $site_record['sync-source']);
    $database = isset($sync_source['database']) && $sync_source['database'] ? $sync_source['database'] : str_replace(array('.', '-'), '_', $sync_source['#name']);
    $command = 'drush @' . $site_record['sync-source'] . " sql-dump > $tmp_file_name";
    buck_shell_exec($command);

    buck_archive_database($site_record);
    buck_destroy_database($site_record);
    buck_create_database($site_record);
    buck_shell_exec('mysql ' . $site_record['database'] . ' < ' . $tmp_file_name);
    buck_color_log('... done.', 'ok');
  }
  else {
    buck_archive_database($site_record);
    buck_destroy_database($site_record);
    buck_create_database($site_record);
    drush_invoke_process(
      '@self',
      'sql-sync',
      array("@" . $site_record['sync-source'], "@" . $site_record['#name']),
      array('--no-cache', '-y', ' --db-su=' . $db_super['user'], '--db-su-pw=' . $db_super['password']),
      array('interactive' => TRUE)
    );
  }
}

/**
 * Archives database.
 */
function buck_archive_database($site_record) {
  $temp_file = '/tmp/' . $site_record['database'] . '-' . time() . '.sql';
  buck_color_log("Archiving the database temporarily ...", 'ok');

  drush_invoke_process(
    '@self',
    'sql-dump',
    array("--result-file=" . $temp_file),
    array("--gzip"),
    array('interactive' => TRUE)
  );

  buck_color_log('... done.', 'ok');
}

/**
 * Creates datase and adds settings.php if they don't exist.
 */
function buck_create_database(&$site_record) {
  if ($site_record['multisite']) {
    $setting_phps = $site_record['root'] . '/sites/' . $site_record['uri'] . '/settings.php';
  }
  else {
    $setting_phps = $site_record['root'] . '/sites/default/settings.php';
  }
  // If there is no local db, need to create one and add it to settings.php
  if (!buck_drush_sql_db_exists($site_record) && !is_readable($settings_phps)) {
    // Create database.
    $site_record['db_pass'] = drush_generate_password();
    $db_url = 'mysql://' . $site_record['dbuser'] . ':' . $site_record['db_pass']  . '@localhost/' . $site_record['database'];

    $user = posix_getpwuid(posix_geteuid());
    $db_super = parse_ini_file($user['dir'] . '/.my.cnf');
    if (!$db_super['user']) {
      drush_set_error("Your .my.cnf file is not setup correctly.");
      return;
    }
    buck_create_settings_php($site_record);
    drush_invoke_process(
      '@self',
      'sql-create',
      array("@" . $site_record['sync-source'], "@" . $site_record['#name']),
      array('--db-url=' . $db_url, '--db-su=' . $db_super['user'], '--db-su-pw=' . $db_super['password']),
      array('interactive' => TRUE)
    );
  }
  else {
    drush_shell_exec('mysql -e "create database ' . $site_record['database'] . '"');
  }
  drush_shell_exec('mysql -e "grant usage on *.* to ' . $site_record['database'] . '@localhost identified by ' . "'" . $site_record['db_pass'] . "'" . '"');
  drush_shell_exec('mysql -e "grant all privileges on ' . $site_record['database'] . '.* to ' . $site_record['database'] . '@localhost"');
}

/**
 * Adds settings.php file.
 */
function buck_create_settings_php($site_record) {
  if ($site_record['multisite']) {
    $folder = $site_record['root'] . '/sites/' . $site_record['uri'];
  }
  else {
    $folder = $site_record['root'] . '/sites/default';
  }
  // Add settings.php
  $twig = buck_create_twig_instance();
  $str_config = $twig->render(
    BUCK_SETTINGS_PHP_TEMPLATE_FILE,
    array(
      'site_record' => $site_record,
    )
  );
  if (!is_readable($folder)) {
    drush_mkdir($folder);
  }

  $config_handle = fopen($folder . '/settings.php', "w+");
  fwrite($config_handle, $str_config);
}

/**
 * Assigns proper permissions to files and settings.php.
 */
function buck_permissions_cleanup($site_record) {
  if (drush_confirm(dt('Skip file permissions check? Not necessary with directory mount on vagrant.'))) {
    return;
  }
  $root = $site_record['root'];
  buck_color_log("Cleaning up permissions ...", 'ok');
  $cmd = 'buck-perms';
  drush_invoke_process(
    '@self',
    $cmd,
    array(),
    array(),
    array('dispatch-using-alias' => TRUE)
  );
  buck_color_log('... done.', 'ok');
}

/**
 * Deletes the sites files using rm -rf.  Also path.old.
 */
function buck_destroy_code($site_record) {
  buck_shell_exec('sudo chmod -R 0755 ' . $site_record['base_root']);
  buck_shell_exec('sudo rm -rf ' . $site_record['base_root']);
}

/**
 * Get the password of your dreams.
 */
function buck_generate_password($length = 10) {
  $chars =  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'.
            '0123456789``-=~!@#$%^&*()_+,.<>?;:[]{}\|';

  $str = '';
  $max = strlen($chars) - 1;

  for ($i=0; $i < $length; $i++)
    $str .= $chars[rand(0, $max)];

  return $str;
}

/**
 * Adds defaultis and configures several alias elements.
 */
function _buck_default_params($site_record) {
  if ($site_record['#group'] == 'pantheon') {
    $site_record['sync-source'] = '@' . $site_record['#name'];
    $site_record['sync-host'] = 'pantheon';
    $site = explode(".", $site_record['#name']);
    $site_record['root'] = '/var/www/' . $site[1];
    $site_record['dbuser'] = str_replace(array('.', '-'), '_', substr($site_record['#name'], 0, 15));
    $site_record['uri'] = $site[1] . '.local';
    $site_record['git'] = array(
      'branch' => 'master',
      'url' => 'ssh://codeserver.' . $site_record['remote_user'] . '@codeserver.' . $site_record['remote_user'] . '.drush.in:2222/~/repository.git',
    );
  }
  $domain = str_replace('/', '', $site_record['uri']);
  if ($site_record['db-name']) {
    $database = $site_record['db-name'];
  }
  else {
    $database = $site_record['#name'] != '@self' ? str_replace(array('.', '-'), '_', $site_record['#name']) : str_replace(array('.', '-'), '_', $domain);
  }
  // These are defaults. They are overwritten if values are already present.
  $defaults['username'] = 'root';
  $defaults['dbpassword'] = '';
  $defaults['database'] = $database;
  $defaults['dbuser'] = $database;
  $defaults['profile'] = 'standard';
  $defaults['db-prefix'] = '';
  $defaults['account-name'] = 'admin';
  $defaults['account-pass'] = 'admin';
  $defaults['account-mail'] = 'admin@' . $domain;
  $defaults['locale'] = 'en';
  $defaults['clean-url'] = '1';
  $defaults['driver'] = 'mysql';
  $defaults['os'] = _drush_get_os();
  $defaults['site-name'] = $domain;
  $defaults['site-mail'] = 'info@'. $domain;
  $defaults['sites-subdir'] = 'default';
  $defaults['sql-dump'] = TRUE;
  // Core defaults. Will be provided if git and profile are not specified.
  $defaults['profile'] = 'standard';
  $defaults['git'] = array(
    'branch' => '7.x',
    'url' => 'http://git.drupal.org/project/drupal.git',
  );
  $defaults['multisite'] = FALSE;

  $defaults['base_root'] = isset($site_record['base_root']) ? $site_record['base_root'] : $site_record['root'];

  // Merge it all together - 2nd array overwrites first.
  $site_record = array_merge($defaults, $site_record);

  if ($site_record['branch']) {
    $site_record['git']['branch'] = $site_record['branch'];
  }
  if ($site_record['webroot']) {
    $site_record['root'] = $site_record['root'] . '/' . $site_record['webroot'];
  }

  // This will be blank unless added to alias.
  $site_record['db-url'] = isset($site_record['db-url']) ? $site_record['db-url'] : 'mysql://' . $site_record['dbuser'] . ':' . $site_record['dbpassword'] . '@localhost/' . $site_record['database'];
  // It might be better to drush_set_option() than return.
  return $site_record;
}

/**
 * Checks site alias against required and verifies several elements.
 */
function _buck_check_params($site_record, $required, $buck_command) {
  // TODO: makefile url and path.
  #$makefile = buck_fixpath("$makefile");
  #if (!file_exists($makefile)) throw new Exception("Makefile ". $makefile ." not found.");
  if (strlen($site_record['database']) > 63) {
    buck_color_log("  '--uri' must be less than 16 characters long, for mysql user name to work.", 'error');
    throw new Exception("Buck could not complete execution.");
  }
  elseif ($buck_command != "buck-perms" && strlen($site_record['database']) < 3) {
    buck_color_log("  '--uri' must be at least 3 characters long.", 'error');
    throw new Exception("Buck could not complete execution.");
  }

  if ($required) {
    // Check required
    foreach($required as $require) {
      if (empty($site_record[$require])) {
        buck_color_log("  '--$require' is a required option for this command.", 'error');
        throw new Exception("Buck could not complete execution.");
      }
    }
  }
  return TRUE;
}

/**
 * List of required parameters for each command.
 */
function _buck_required_params($command) {
  $required = array(
    'buck' => array(
      'root',
      'uri',
    ),
    'buck-destroy' => array(
      'database',
      'db-url',
      'root',
      'uri',
    ),
    'buck-perms' => array(
      'root',
    ),
    'buck-sql' => array(
      'sync-source',
    ),
  );

  return $required[$command];
}

/**
 * Gets alias info and overwrites with added options.
 * Checks that alias exists and enough info is supplied.
 */
function _buck_env_prepare($site_record) {
  // Allow cli args to override site alias elements.
  $args = drush_get_context('cli');
  $site_record = array_merge($site_record, $args);

  $cmd = drush_get_arguments();

  // Grab command from arguments.
  $buck_command = array_shift($cmd);

  // Add defaults.
  $site_record = _buck_default_params($site_record);

  $required = _buck_required_params($buck_command);

  // Check that required params are inlcuded and that some params are acceptable.
  _buck_check_params($site_record, $required, $buck_command);
  return $site_record;
}

/**
 * Exactly the same as drush_shell_cd_and_exec except for call to buck_passthru_shell_exec().
 */
function buck_passthru_shell_cd_and_exec($effective_wd, $cmd) {
  $args = func_get_args();

  $effective_wd = array_shift($args);
  $cwd = getcwd();
  drush_op('chdir', $effective_wd);
  $result = call_user_func_array('buck_passthru_shell_exec', $args);
  drush_op('chdir', $cwd);
  return $result;
}

/**
 * Helper function for executing shell commands.  Throws exceptions on error.
 *
 * @param <type> $cmd the command to run
 * @param <type> $throwexception throw exception on error?
 */
function buck_shell_exec($cmd, $throwexception = TRUE) {
  // Note: for shell commands: 0=success, non-zero=error code
  $ret = drush_shell_exec($cmd);
  if ($throwexception && !$ret) {
    $msg = "Command returned unexpected result: $cmd";
    $output = drush_shell_exec_output();
    foreach ($output as $line) {
      $msg="\n  $line";
    }
    throw new Exception($msg);
  }
}

/**
 * Exactly the same as drush_shell_exec except for call to _buck_passthru_shell_exec().
 */
function buck_passthru_shell_exec($cmd) {
  return _buck_passthru_shell_exec(func_get_args());
}

/**
 * Same as _drush_shell_exec() except calls to passthru() instead of exec();
 * Want the option of letting user know what is going on.
 */
function _buck_passthru_shell_exec($args, $interactive = FALSE) {
  $result = FALSE;
  // Do not change the command itself, just the parameters.
  for ($x = 1; $x < sizeof($args); $x++) {
    $args[$x] = drush_escapeshellarg($args[$x]);
  }
  // Important: we allow $args to take one of two forms here.  If
  // there is only one item in the array, it is the already-escaped
  // command string, but otherwise sprintf is used.  In the case
  // of pre-escaped strings, sprintf will fail if any of the escaped
  // parameters contain '%', so we must not call sprintf unless necessary.
  if (count($args) == 1) {
    $command = $args[0];
  }
  else {
    $command = call_user_func_array('sprintf', $args);
  }

  if (drush_get_context('DRUSH_VERBOSE') || drush_get_context('DRUSH_SIMULATE')) {
    drush_print('Executing: ' . $command, 0, STDERR);
  }
  if (!drush_get_context('DRUSH_SIMULATE')) {
    if ($interactive) {
      $result = drush_shell_proc_open($command);
      return ($result == 0) ? TRUE : FALSE;
    }
    else {
      // Only change from _drush_shell_exe().
      passthru($command . ' 2>&1', $output);
      _drush_shell_exec_output_set($output);

      if (drush_get_context('DRUSH_DEBUG')) {
        foreach ($output as $line) {
          drush_print($line, 2);
        }
      }

      // Exit code 0 means success.
      return ($result == 0);
    }
  }
  else {
    return TRUE;
  }
}

/**
 * I like pretty colors.
 */
function buck_color_log($message, $type = 'notice', $error = null, $color = '33') {
  $message = "\033[" . $color . "m" . $message . "\033[37m \n";
  drush_log($message, $type, $error);
}

/**
 * The following commands used to be in drush but got removed in 7.x
 *
 * Instead of being smart and fixing that by trying to adopt their new functions
 * we are being dumb.
 */

function buck_drush_sql_db_exists($db_spec) {
  if  ($db_spec['driver'] == 'sqlite') {
    return file_exists($db_spec['database']);
  }

  $connect_yes_db = buck_drush_sql_connect($db_spec);
  $database = $db_spec['database'];
  unset($db_spec['database']);
  $connect_no_db = buck_drush_sql_connect($db_spec);
  // We need the output back so we can't use drush_sql_query().
  switch ($db_spec['driver']) {
    case 'mysql':
      $sql = "SELECT 1;";
      // Suppress ugly output. Redirect STDERR and STDOUT. We just need exit code.
      $bit_bucket = drush_bit_bucket();
      return drush_shell_exec("$connect_yes_db -e \"$sql\" 2> $bit_bucket > $bit_bucket");
    case 'pgsql':
      $sql = "SELECT 1 AS result FROM pg_database WHERE datname='$database'";
      drush_shell_exec("$connect_no_db -t -c \"$sql\"");
      $output = drush_shell_exec_output();
      return (bool)$output[0];
    case 'sqlsrv':
      // TODO: untested, but the gist is here.
      $sql = "if db_id('$database') IS NOT NULL print 1";
      drush_shell_exec("$connect_no_db -Q \"$sql\"");
      $output = drush_shell_exec_output();
      return $output[0] == 1;
  }
}

function buckbuck_drush_sql_get_scheme($db_spec = NULL) {
  if (is_null($db_spec)) {
    $db_spec = _drush_sql_get_db_spec();
  }
  return $db_spec['driver'];
}

function buck_drush_sql_connect($db_spec = NULL) {
  switch (buckbuck_drush_sql_get_scheme($db_spec)) {
    case 'mysql':
      $command = 'mysql';
      if (drush_get_option('A', FALSE)) {
        $command .= ' -A';
      }

      break;
    case 'pgsql':
      $command = 'psql';
      break;
    case 'sqlite':
      $command = 'sqlite3';
      break;
    case 'sqlsrv':
      $command = 'sqlcmd';
      break;
    case 'oracle':
      // use rlwrap if available for readline support
      if ($handle = popen('rlwrap -v', 'r')) {
        $command = 'rlwrap sqlplus';
        pclose($handle);
      }
      else {
        $command = 'sqlplus';
      }
      break;
  }
  $command .= buck_drush_sql_get_credentials($db_spec);
  return $command;
}

function buck_drush_sql_get_scheme($db_spec = NULL) {
  if (!isset($db_spec)) {
    $db_spec = _drush_sql_get_db_spec();
  }
  return $db_spec['driver'];
}

function buck_drush_sql_get_credentials($db_spec = NULL) {
  if (!isset($db_spec)) {
    $db_spec = _drush_sql_get_db_spec();
  }

  // Build an array of key-value pairs for the parameters.
  $parameters = array();

  switch (buck_drush_sql_get_scheme($db_spec)) {
    case 'mysql':
      // Some drush commands (e.g. site-install) want to connect to the
      // server, but not the database.  Connect to the built-in database.
      $parameters['database'] = empty($db_spec['database']) ? 'information_schema' : $db_spec['database'];

      // Default to unix socket if configured.
      if (!empty($db_spec['unix_socket'])) {
        $parameters['socket'] = $db_spec['unix_socket'];
      }
      // EMPTY host is not the same as NO host, and is valid (see unix_socket).
      elseif (isset($db_spec['host'])) {
        $parameters['host'] = $db_spec['host'];
      }

      if (!empty($db_spec['port'])) {
        $parameters['port'] = $db_spec['port'];
      }

      // User is required. Drupal calls it 'username'. MySQL calls it 'user'.
      $parameters['user'] = $db_spec['username'];

      // EMPTY password is not the same as NO password, and is valid.
      if (isset($db_spec['password'])) {
        $parameters['password'] = $db_spec['password'];
      }
      break;

    case 'pgsql':
      // Some drush commands (e.g. site-install) want to connect to the
      // server, but not the database.  Connect to the built-in database.
      $parameters['dbname'] = empty($db_spec['database']) ? 'template1' : $db_spec['database'];

      // Host and port are optional but have defaults.
      $parameters['host'] = empty($db_spec['host']) ? 'localhost' : $db_spec['host'];
      $parameters['port'] = empty($db_spec['port']) ? '5432' : $db_spec['port'];

      // Username is required.
      $parameters['username'] = $db_spec['username'];

      // Don't set the password.
      // @see http://drupal.org/node/438828
      break;

    case 'sqlite':
      // SQLite doesn't do user management, instead relying on the filesystem
      // for that. So the only info we really need is the path to the database
      // file, and not as a "--key=value" parameter.
      return ' '  .  $db_spec['database'];
      break;

    case 'sqlsrv':
      // Some drush commands (e.g. site-install) want to connect to the
      // server, but not the database.  Connect to the built-in database.
      $database = empty($db_spec['database']) ? 'master' : $db_spec['database'];
      // Host and port are optional but have defaults.
      $host = empty($db_spec['host']) ? '.\SQLEXPRESS' : $db_spec['host'];
      return ' -S ' . $host . ' -d ' . $database . ' -U ' . $db_spec['username'] . ' -P ' . $db_spec['password'];
      break;


    case 'oracle':
      // Return an Oracle connection string
      return ' ' . $db_spec['username'] .'/' . $db_spec['password'] . ($db_spec['host']=='USETNS' ? '@' . $db_spec['database'] : '@//' . $db_spec['host'] . ':' . ($db_spec['port'] ? $db_spec['port'] : '1521') . '/' . $db_spec['database']);
      break;

  }

  // Turn each parameter into a valid parameter string.
  $parameter_strings = array();
  foreach ($parameters as $key => $value) {
    // Only escape the values, not the keys or the rest of the string.
    $value = drush_escapeshellarg($value);
    $parameter_strings[] = "--$key=$value";
  }

  // Join the parameters and return.
  return ' ' . implode(' ', $parameter_strings);
}

function buck_create_twig_instance() {
  require_once 'vendor/autoload.php';
  $templates_dir = implode('/', array(dirname(__FILE__), 'templates'));
  $loader = new Twig_Loader_Filesystem($templates_dir);
  $twig = new Twig_Environment($loader, array('cache' => drush_tempdir()));
  return $twig;
}

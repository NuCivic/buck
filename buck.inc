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

/**
 * This is the virtual hosts template file for the apache functions.
 */
define('BUCK_APACHE_TEMPLATE', "
<Directory #CODEPATH#>

  Options FollowSymLinks
  AllowOverride None

  # Protect files and directories from prying eyes.
  <FilesMatch \"\.(engine|inc|info|install|make|module|profile|test|po|sh|.*sql|theme|tpl(\.php)?|xtmpl)$|^(\..*|Entries.*|Repository|Root|Tag|Template)$\">
    Order allow,deny
  </FilesMatch>

  # Hide these from public view.
  <FilesMatch \"(^LICENSE|CHANGELOG|MAINTAINERS|INSTALL|UPGRADE|API|README).*\.txt$\">
    Order deny,allow
    Deny from all 
  </FilesMatch>

  RewriteEngine on
  RewriteRule \"(^|/)\.\" - [F]
  RewriteCond %{REQUEST_FILENAME} !-f
  RewriteCond %{REQUEST_FILENAME} !-d
  RewriteCond %{REQUEST_URI} !=/favicon.ico
  RewriteRule ^ index.php [L]

  RewriteCond %{HTTP:Accept-encoding} gzip
  RewriteCond %{REQUEST_FILENAME}\.gz -s
  RewriteRule ^(.*)\.css $1\.css\.gz [QSA]

  # Serve gzip compressed JS files if they exist and the client accepts gzip.
  RewriteCond %{HTTP:Accept-encoding} gzip
  RewriteCond %{REQUEST_FILENAME}\.gz -s
  RewriteRule ^(.*)\.js $1\.js\.gz [QSA]

  # Serve correct content types, and prevent mod_deflate double gzip.
  RewriteRule \.css\.gz$ - [T=text/css,E=no-gzip:1]
  RewriteRule \.js\.gz$ - [T=text/javascript,E=no-gzip:1]

  <FilesMatch \"(\.js\.gz|\.css\.gz)$\">
    # Serve correct encoding type.
    Header set Content-Encoding gzip
    # Force proxies to cache gzipped & non-gzipped css/js files separately.
    Header append Vary Accept-Encoding
  </FilesMatch>

</Directory>

<VirtualHost *:80>
  ServerName #DOMAIN#
  DocumentRoot #CODEPATH#
  LogLevel warn
  ServerSignature Off
</VirtualHost>
");

/**
 * This is the virtual hosts template file for settings.php file.
 */
define('BUCK_SETTINGS_PHP_TEMPLATE', "<?php
\$databases = array (
  'default' =>
  array (
    'default' =>
    array (
      'database' => '#DATABASE#',
      'username' => '#USERNAME#',
      'password' => '#PASSWORD#',
      'host' => 'localhost',
      'port' => '',
      'driver' => 'mysql',
      'prefix' => '',
    ),
  ),
);
\$update_free_access = FALSE;
\$base_url = '#URL#';
ini_set('session.gc_probability', 1);
ini_set('session.gc_divisor', 100);
ini_set('session.gc_maxlifetime', 200000);
ini_set('session.cookie_lifetime', 2000000);
\$conf['404_fast_paths_exclude'] = '/\/(?:styles)\//';
\$conf['404_fast_paths'] = '/\.(?:txt|png|gif|jpe?g|css|js|ico|swf|flv|cgi|bat|pl|dll|exe|asp)$/i';
\$conf['404_fast_html'] = '<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL \"@path\" was not found on this server.</p></body></html>';
drupal_fast_404();
");

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
    drush_log("Creating DNS config (add $domain to /etc/hosts) ...", 'ok');
    buck_shell_exec("echo '127.0.0.1 $domain #' | sudo tee -a /etc/hosts");
    drush_log('... done.', 'ok');
  }
  else {
    drush_log("DNS config $domain already added to /etc/hosts", 'ok');
  }
}

/**
 * Destroy the DNS.  Edit /etc/hosts file on local machine
 */
function buck_destroy_dns($site_record) {
  $domain = $site_record['uri'];
  buck_shell_exec("sudo sed -i \"/ $domain .*#/d\" /etc/hosts");
}

/**
 * Configure apache virtualhost file using the template.
 */
function buck_create_webserver_config($site_record) {
  $domain = $site_record['uri'];
  if (file_exists("/etc/apache2/sites-available/$domain")) {
    if (!drush_confirm(dt('Vhost ' .  $domain . ' already exists. Do you want to erase its contents?'))) {
      return drush_set_error(0, dt('You did not wish to overwrite the vhost. Aborting.'));
    }
  }

  $str_config = BUCK_APACHE_TEMPLATE;

  drush_log("Creating apache config (/etc/apache2/sites-enabled/$domain)", 'ok');

  $str_config = str_replace("#DOMAIN#", $domain, $str_config);
  $str_config = str_replace("#CODEPATH#", $site_record['root'], $str_config);

  $config_handle = fopen("/etc/apache2/sites-available/$domain", "w+");
  fwrite($config_handle, $str_config);

  buck_shell_exec("sudo a2ensite $domain");

  // restart apache
  buck_shell_exec("sudo /etc/init.d/apache2 reload");

  drush_log('... done.', 'ok');
}

/**
 * Destroy the apache virtual hosts config. a2dissite, then rm file.
 */
function buck_destroy_webserver_config($site_record) {
  $domain = $site_record['uri'];
  buck_shell_exec("sudo a2dissite $domain", false);
  $filename = "/etc/apache2/sites-available/$domain";
  if (file_exists($filename)) unlink($filename);  // unlink = delete file
  drush_log("Removing apache config (/etc/apache2/sites-enabled/$domain)", 'ok');
  buck_shell_exec("sudo /etc/init.d/apache2 restart");
}

/**
 * Destroys the database using SQL.
 */
function buck_destroy_database($site_record) {
  drush_log('Destroying database and user ' . $site_record['database'] . ' ...', 'ok');
  // There is no sql-destroy in drush.
  drush_shell_exec('mysql -e "drop database ' . $site_record['database'] . '"');
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
    if (!drush_confirm(dt('Folder ' . $site_record['base_root'] . ' already exists. Do you want to remove it?'))) {
      return;
    }
    if (file_exists($oldpath)) {
      drush_log("Removing existing directory $oldpath", "ok");
      drush_shell_exec("rm -rf $oldpath");
      drush_log("... done", "ok");
    }
    drush_log("Archiving existing directory $codepath to $tar_file", "ok");
    buck_shell_exec("/bin/tar zcfP $tar_file $codepath");
    drush_log("Removing existing directory $codepath", "ok");
    drush_delete_dir("$codepath");
    drush_log("... done", "ok");
    drush_log("Creating directory $codepath", "ok");
    drush_mkdir($site_record['base_root']);
    drush_log("... done", "ok");
  }
  else {
    drush_log("Creating directory $codepath", "ok");
    drush_mkdir($site_record['base_root']);
    drush_log("... done", "ok");
  }
}

/**
 * Removes code, media and other files.
 */
function buck_destroy_root($site_record) {
  $root = $site_record['base_root'];
  drush_log("Removing existing directory $root", "ok");
  drush_shell_exec("rm -rf $root");
  drush_log("... done", "ok");
}

/**
 * Updates git code.
 */
function buck_git_pull($directory) {
  // TODO: Check to make sure that there are no
  // uncommitted changes etc.
  $command = 'git pull --rebase';
  drush_shell_cd_and_exec($directory, 'git pull --rebase');
}

/**
 * Updates existing profile managed by git.
 */
function buck_profile_git($site_record) {
  $profile_dir = $site_record['root'] . '/profiles/' . $site_record['profile'];
  // Remove files if not under version control.
  buck_profile_make_clean($profile_dir);
  buck_git_update($profile_dir, $site_record['git']);

  // First look for .gitignore
  $git_ignore = $profile_dir . '/.gitignore';
  if (file_exists($git_ignore)) {
    $profile_ignored = file($git_ignore);
    foreach ($profile_ignored as $dir) {
      drush_delete_dir($profile_dir . '/' . trim($dir));
    }
    $tmp_dir = drush_tempdir() . '/' . $site_record['profile'];
    $make = drush_invoke_process(
      '@self',
      'make',
      array($site_record['makefile'], $tmp_dir),
      array('--no-core')
    );

    // Move ignore files from temp directory back into profile.
    foreach ($profile_ignored as $dir) {
      $ignore_dir = $tmp_dir . "/profiles/" . $site_record['profile'] . '/' . trim($dir);
      drush_move_dir($ignore_dir, $profile_dir . '/' . trim($dir), TRUE);
    }
  }
  // Second look for profile.rebuild.sh .
  elseif(file_exists($profile_dir . '/' . $site_record['profile'] . '.rebuild.sh')) {
    drush_log(dt('Could not find .gitignore file rebuilding with rebuild.sh.'));
    drush_shell_cd_and_exec($profile_dir, 'sh ' . $site_record['profile'] . '.rebuild.sh');
  }
  else {
    throw new Exception("Could not find .gitignore or rebuild.sh.");
  }
}

/**
 * This is my silly version of module_invoke_all() but instead it takes
 * over execution completely if it has a function to return.
 */
function buck_host_include($site_record, $function) {
  if (isset($site_record['sync-host']) && function_exists($site_record['sync-host'] . '_buck_sync_files')) {
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
    array('invoke' => TRUE),
    array('interactive' => TRUE)
  );
  drush_log("... done", "ok");
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
  buck_git_update($site_record['root'], $site_record['git']);
}

/**
 * Updates a git repo or clones if it doesn't exist.
 */
function buck_git_update($git_dir, $git_args) {
  if (file_exists($git_dir . '/.git')) {
    drush_log('Git repo at ' . $git_dir . '/.git already exists. Updating...', 'ok');
    buck_git_pull($git_dir);
    drush_log("... done", "ok");
    return ;
  }
  $branch = isset($git_args['branch']) ? '--branch ' . $git_args['branch'] : '';
  $command = 'git clone ' . $branch . ' ' . $git_args['url'] . ' ' . $git_dir;
  drush_log('Downloading code to ' . $git_dir . ' (takes a minute, check network activity) ...', 'ok');
  if (!system($command)) {
    return drush_set_error('DRUSH_PM_GIT_CHECKOUT_PROBLEMS', dt('Unable to clone project.'));
  }
  drush_log("... done", "ok");
}

/**
 * Installs site using profile.
 */
function buck_profile_install($site_record) {
  drush_log('Installing ' . $site_record['profile'] . ' ...', 'ok');


  $sql_connect = drush_invoke_process(
    '@self',
    'sql-connect'
  );
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
  if (is_array($sql_connect)) {
    $db_pass = drush_generate_password();
    $opts[] = '--db-url=mysql://' . $site_record['dbuser'] . ':' . $db_pass . '@localhost/' . $site_record['database'];
  }
  drush_invoke_process(
    '@self',
    'site-install',
    array($site_record['profile']),
    $opts,
    array('#integrate' => TRUE)
  );
  drush_log("... done", "ok");
}

/**
 * Makes site via make file.
 */
function buck_profile_make($site_record) {
  if (isset($site_record['makefile'])) {
    if (!file_exists($site_record['base_root'])) {
      buck_create_root($site_record);
    }

    drush_log("Downloading and installing through make file ...", 'ok');
    $make = drush_invoke_process(
      '@self',
      'make',
      array($site_record['makefile'], $site_record['root']),
      array('--prepare-install')
    );
    drush_log('... done.', 'ok');
  }
}

/**
 * Removes profile files so we can use in version control.
 * This only occurs upon first installation.
 */
function buck_profile_make_clean($profile_dir) {
  if (!file_exists($profile_dir . '/.git') && file_exists($profile_dir)) {
    drush_delete_dir($profile_dir);
  }
}

/**
 * Syncs your database from a remote database.
 */
function buck_sync_database($site_record) {
  if (buck_host_include($site_record, __FUNCTION__)) {
    return;
  }
 
  buck_create_database($site_record);
  drush_invoke_process(
    '@self',
    'sql-sync',
    array("@" . $site_record['sync-source'], "@" . $site_record['#name']),
    array('--target-databsae=' . $site_record['database'],  '--db-su=' . $db_super['user'], '--db-su-pw=' . $db_super['password']),
    array('interactive' => TRUE)
  );
}

/**
 * Creates datase and adds settings.php if they don't exist.
 */
function buck_create_database(&$site_record) {
  // If there is no local database, need to create one and add it to settings.php
  if (!drush_sql_db_exists($site_record) || !file_exists($site_record['root'] . '/sites/default/settings.php')) {
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

    $config_handle = fopen($site_record['root'] . "/sites/default/settings.php", "w+");
    fwrite($config_handle, $str_config);
  }
}

/**
 * Assigns proper permissions to files and settings.php.
 */
function buck_permissions_cleanup($site_record) {
  $root = $site_record['root'];
  drush_log("Cleaning up permissions ...", 'ok');
  $cmd = 'buck-fixperms';
  drush_invoke_process(
    '@self',
    $cmd
  );
  drush_log('... done.', 'ok');
}

/**
 * Deletes the sites files using rm -rf.  Also path.old.
 */
function buck_destroy_code($site_record) {
  $codepath = $site_record['base_root'];
  buck_shell_exec("rm -rf $codepath");
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
  // Should use drush_set_option() for these:
  $domain = str_replace('/', '', $site_record['uri']);
  $database = $site_record['#name'] != '@self' ? str_replace(array('.', '-'), '_', $site_record['#name']) : str_replace(array('.', '-'), '_', $domain);
  // These are defaults. They are overwritten if values are already present.
  $defaults['username'] = 'root';
  $defaults['dbpassword'] = '';
  $defaults['database'] = $database;
  $defaults['dbuser'] = $database;
  $defaults['profile'] = 'standard';
  $defaults['db-prefix'] = '';
  $defaults['account-name'] = 'admin';
  $defaults['account-pass'] = 'admin';
  $defaults['account-mail'] = 'admin@'. $domain;
  $defaults['locale'] = 'en';
  $defaults['clean-url'] = '1';
  $defaults['driver'] = 'mysql';
  $defaults['site-name'] = $domain;
  $defaults['site-mail'] = 'info@'. $domain;
  $defaults['sites-subdir'] = 'default';
  // Core defaults. Will be provided if git and profile are not specified.
  $defaults['profile'] = 'standard';
  $defaults['git'] = array(
    'branch' => '7.x',
    'url' => 'http://git.drupal.org/project/drupal.git',
  );
  $defaults['base_root'] =  preg_replace('/' . BUCK_WEBROOT  . '$/', '', $site_record['root']);

  // Merge it all together - 2nd array overwrites first.
  $site_record = array_merge($defaults, $site_record);

  // This will be blank unless added to alias.
  $site_record['db-url'] = $site_record['db-url'] ? $site_record['db-url'] : 'mysql://' . $site_record['dbuser'] . ':' . $site_record['dbpassword'] . '@localhost/' . $site_record['database'];
  // It might be better to drush_set_option() than return.
  return $site_record;
}

/**
 * Checks site alias against required and verifies several elements.
 */
function _buck_check_params($site_record,  $required) {
  // TODO: makefile url and path.
  #$makefile = buck_fixpath("$makefile");
  #if (!file_exists($makefile)) throw new Exception("Makefile ". $makefile ." not found.");
  if (strlen($site_record['database']) > 15) {
    drush_log("  '--uri' must be less than 16 characters long, for mysql user name to work.", 'error');
    throw new Exception("Buck could not complete execution.");
  }
  elseif (strlen($site_record['database']) < 3) {
    drush_log("  '--uri' must be at least 3 characters long.", 'error');
    throw new Exception("Buck could not complete execution.");
  }

  // Check required
  foreach($required as $require) {
    if (empty($site_record[$require])) {
      drush_log("  '--$require' is a required option for this command.", 'error');
      throw new Exception("Buck could not complete execution.");
    }
  }
  return $pass;
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
    'buck-fixperms' => array(
      'root',
    ),
  );

  return $required[$command];
}

/**
 * Gets alias info and overwrites with added options.
 * Checks that alias exists and enough info is supplied.
 */
function _buck_env_prepare($site_record) {
  $args = drush_get_arguments();
  // Grab command from arguments.
  $buck_command = array_shift($args);

  // Add defaults.
  $site_record = _buck_default_params($site_record);

  $required = _buck_required_params($buck_command);

  // Check that required params are inlcuded and that some params are acceptable.
  _buck_check_params($site_record, $required);
  return $site_record;
}


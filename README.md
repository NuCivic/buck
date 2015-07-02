This is a Drush extension that creates local sites from variables in drush alias files.

This is a simple as posssible wrapper around Drush commands with a few add-ons. It works only on Ubuntu. Though that is being expanded. For something
more feature-rich and extensible see: http://drupal.org/project/fetcher

# Install

Easiest way to install is to clone this repo inside `~/.drush`

```
cd ~/.drush
git clone git@github.com:NuCivic/buck.git
cd buck
composer install
```

# Commands

### Create or update site:

```drush buck @mysite```

Creates or updates webroot, code, virtual host, /etc/hosts, database and files. It detects if an installation already exists and creates it if it doesn't and updates it if it does. Also fixes permisisons where needed.

### Create or update site element:

```drush buck-command [command] @mysite```

Commands include webserver_config, hosts, databaset, code, and files.

### Destory site:

```drush buck destroy @mysite```

# Types of Sites

Buck handles two types of sites.

## 1) Installation profiles

This requires the following in a alias:

```'profile' => 'name of profile',
'makefile' => 'link to makefile',```

Optional but helpful:

```'site-name' => 'Name of site',```

### 2) "Regular" Drupal sites

This requires the following in a drush alias:

```'sync-source' => 'name of alias that this site should sync from',```

# Required Elements

Buck requires the following elements:

```
  'git' => array(
   'url' => '[git url]',
   'branch' => '[git branch]',
  ),  
  'root' => '[location of site on server]',
  'uri' => '[url]', 
  // Required for "Regular" sites that sync from a non-local source.
  'remote-host' => '[CNAME or IP address of remote server]',
  'remote-user' => '[Remote server user name]',
  'path-aliases' => array(
    '%files' => 'sites/default/files',
    '%dump-dir' => '/tmp',
  ),
```
# Optional Elements

### 'features-revert'

If this is included, selected features will be reverted after install.

Revert all features:
``` 'features_revert' => TRUE, ```

Revert selected features:

``` 'features_revert' => array('feature1', 'feature2'), ```

# Example Alias

This creates a local version of the DKAN distro:

```
$aliases['dkan.local'] = array(
  'git' => array(
    'url' => 'http://git.drupal.org/project/dkan.git',
    'branch' => 'master',
  ),
  'root' => '/var/www/dkan',
  'makefile' => 'http://drupalcode.org/project/dkan.git/blob_plain/refs/heads/master:/dkan_distro.make',
  'profile' => 'dkan',
  'uri' => 'dkan.local',
  'path-aliases' => array(
    '%files' => 'sites/default/files',
    '%dump-dir' => '/tmp',
  ),
  'sync-source' => 'dkan.live',
);
```

# Dumb Assumptions

User who initiates command needs the following permissions:

```
webadmin ALL = NOPASSWD: /etc/init.d/apache2
webadmin ALL = NOPASSWD: /usr/sbin/a2ensite
webadmin ALL = NOPASSWD: /usr/sbin/dissite
webadmin ALL = NOPASSWD: /usr/bin/tee -a /etc/hosts
webadmin ALL = NOPASSWD: /bin/sed -i /etc/hosts
```

# Extending

If you include 'sync-host' you can override the behavior of the sync_files and
sync_database for selected hosts. Include a example.buck.inc file and it will
autload the commands.

See pantheon.buck.inc for an example.

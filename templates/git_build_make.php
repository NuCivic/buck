api = 2
core = 7.x

includes[core] = drupal-org-core.make

; Profile

projects[{{site_record.profile}}][type] = profile
projects[{{site_record.profile}}][download][type] = git
projects[{{site_record.profile}}][download][url] = {{site_record.git.url}}
projects[{{site_record.profile}}][download][branch] = {{site_record.git.branch}}

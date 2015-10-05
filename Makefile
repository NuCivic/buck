.PHONY: default test prepare clean

DATA_STARTER_ALIASES_URL := https://raw.githubusercontent.com/NuCivic/data_starter/master/assets/drush/datastarter.aliases.drushrc.php
DRUSH_PATH := vendor/drush/drush

default: clean prepare test

clean:
	rm -fR $(DRUSH_PATH)/commands/core/buck
	rm $(DRUSH_PATH)/datastarter.aliases.drushrc.php

prepare:
	mkdir -p $(DRUSH_PATH)/commands/core/buck
	curl $(DATA_STARTER_ALIASES_URL) > $(DRUSH_PATH)/datastarter.aliases.drushrc.php
	git archive HEAD | tar -x -C $(DRUSH_PATH)/commands/core/buck/
	cp composer.json.stub $(DRUSH_PATH)/commands/core/buck/composer.json
	cd $(DRUSH_PATH)/commands/core/buck/ && composer install

test:
	phpunit --configuration="$(DRUSH_PATH)/tests" tests

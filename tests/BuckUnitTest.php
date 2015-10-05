<?php
/**
 * @file
 * Buck Unit tests.
 */

namespace Unish;

/**
 * Class  BuckPermTest.
 *
 * @group commands
 */
class BuckPermTest extends CommandUnishTestCase {

  /**
   * Run basic buck command.
   */
  public function testBasicBuck() {
    $this->drush('buck', array(
      '@datastarter.local',
      '--root=/var/www/data_starter_test',
    ));
  }

}

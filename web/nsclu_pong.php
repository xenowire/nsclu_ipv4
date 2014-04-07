<?php
require_once( 'nsclu_conf.php' );
header( 'X-xenowire-app: nsclu' );
header( 'X-xenowire-nsclu-count: ' . nsclu_count(FALSE) );
?>
<?php
// http://192.168.2.2/post_log.php?chipid=123&msg="My log message"
require_once('dbconfig.php');

$chipid   = isset($_GET['chipid'])   ? $_GET['chipid']      : '';
$msg    = isset($_GET['msg'])   ? $_GET['msg']      : 'N/A';

if ($chipid == ''){
	echo "Missing chipid<br>";
	exit(); //No use to save this
}

$mysql = new mysqli("localhost", DB_USER, DB_PASSWORD, DB_DATABASE);

if ($mysqli -> connect_errno) {
	echo "Failed to connect to MySQL: " . $mysqli -> connect_error;
	exit();
  }

$query = <<<EOD
INSERT INTO Logging (chipid, msg, time)
VALUES ('{$chipid}', '{$msg}', Now())
EOD;

$res = $mysql->query($query);
$mysql->close();
<?php
// http://192.168.2.2/post_value.php?chipid=123&name=distr&value=32.5&unit=cm
require_once('dbconfig.php');

$chipid   = isset($_GET['chipid'])   ? $_GET['chipid']      : '';
$name    = isset($_GET['name'])   ? $_GET['name']      : '';
$value    = isset($_GET['value'])   ? $_GET['value']      : '';
$unit    = isset($_GET['unit'])   ? $_GET['unit']      : 'N/A';


if ($chipid == '' || $name== '' || $value == ''){
	echo "Missing parameters<br>";
	exit(); //No use to save this
}

$mysql = new mysqli("localhost", DB_USER, DB_PASSWORD, DB_DATABASE);

if ($mysqli -> connect_errno) {
	echo "Failed to connect to MySQL: " . $mysqli -> connect_error;
	exit();
  }

$query = <<<EOD
INSERT INTO Measurement (chipid, name, value, unit, time)
VALUES ('{$chipid}', '{$name}',{$value},'{$unit}', Now())
EOD;

$res = $mysql->query($query);
$mysql->close();
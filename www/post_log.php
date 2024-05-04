<?php
require_once('dbconfig.php');

$chipid   = isset($_GET['chipid'])   ? $_GET['chipid']      : '';
$log    = isset($_GET['log'])   ? $_GET['log']      : 'N/A';

$mysql = new mysqli(DB_HOST, DB_USER, DB_PASSWORD, DB_DATABASE);
                $mysql->set_charset("utf8");
		if (mysqli_connect_error()) {
   			echo "Connect to database failed: ".mysqli_connect_error()."<br>";
   			exit();
		}

$query = <<<EOD
INSERT INTO Logging (chipid, time, msg)
VALUES ('{$chipid}', '{$log}', Now())
EOD;


$res = $mysql->query($query);
$mysql->close();
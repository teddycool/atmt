<html>
<head>
</head>
<body>
<h1> 2024 Scania Hack: N&ouml;&ouml;rds </h1>
<div>
<a href=index.php>HOME</a>
</div>
<h2>Logging</h2>
<form action="view_logs.php" method=POST>
<div>
<label for="ChipID">Choose ChipID  </label>
<select name="ChipID" id="ChipID">
<?php 
$conn = mysqli_connect('localhost','root','hack1','scaniahack');
$conn = new mysqli('localhost','root','hack1','scaniahack');
if ($conn->connect_error) {
	die("Connection failed: " . $conn->connect_error);
}

$sql = "SELECT DISTINCT chipid FROM Logging;";

$result = $conn->query($sql);
if ($result->num_rows > 0) {
  // output data of each row
  while($row = $result->fetch_assoc()) {
    echo "<option value=" . $row['chipid'] .">" . $row['chipid'] . "</option>";
  }
  echo "</select>";
} else {
  echo "0 results";
}
$conn->close();
?>
</div>
<div>
<button type=submit>Show Logs</button></form>
</div>
<?php if ($_SERVER["REQUEST_METHOD"] == "POST"){
echo "<h3>Logs for ChipID ".$_POST['ChipID']."</h3>";
$conn = mysqli_connect('localhost','root','hack1','scaniahack');
$conn = new mysqli('localhost','root','hack1','scaniahack');
if ($conn->connect_error) {
	die("Connection failed: " . $conn->connect_error);
}
$sql = "SELECT time,msg FROM Logging where chipid=\"".$_POST['ChipID']."\" ORDER BY id DESC limit 1000;";
$result = $conn->query($sql);
if ($result->num_rows > 0) {
	while($row = $result->fetch_assoc()) {
		echo "<br>".$row['time']."  ".$row['msg'] ;
	}
}
}
?>
</body>
</html>


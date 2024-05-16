<html>
<head>
</head>

<body>
<h2>This is edited from Linux on a smb mounted share</h2>

<p>To mount the www folder follwo the next two steps
<br>mkdir ~/www
<br>sudo mount -t cifs -o uid=$(id -u),gid=$(id -g),forceuid,forcegid //192.168.2.2/hack-www www/

<p>To mount the storage folder follwo the next two steps
<br>mkdir ~/storage
<br>sudo mount -t cifs -o uid=$(id -u),gid=$(id -g),forceuid,forcegid //192.168.2.2/hack-storage storage/
<div>
  <a href=view_logs.php>To view the logs click here</a>
</div>
<div>
  <a href=http://192.168.2.3/update>To OTA 192.168.2.3</a>
  <a href=http://192.168.2.4/update>To OTA 192.168.2.4</a>
</div>
</body>

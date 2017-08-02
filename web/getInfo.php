
<?php
$filename=$_GET["filename"];
shell_exec("chmod +x readFile.sh");
$output = shell_exec("./readFile.sh {$filename}");
echo "<pre>$output</pre>";
?>
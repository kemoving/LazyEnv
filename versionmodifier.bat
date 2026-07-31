$old = "0.6.0"; $new = "0.6.0"
$oldRC = $old -replace '\.', ','
$newRC = $new -replace '\.', ','
foreach ($f in @("CMakeLists.txt","LazyEnv.rc","README.md","resources\index.html","resources\script.js")) {
    $p = "E:\Dev\LazyEnv\$f"
    (Get-Content $p -Raw) -replace [regex]::Escape($old), $new -replace [regex]::Escape("$oldRC,0"), "$newRC,0" | Set-Content $p -NoNewline
}

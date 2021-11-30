$FileBrowser = New-Object System.Windows.Forms.OpenFileDialog
$FileBrowser.Title = "Select ArmaExtensionInterface Project" 
$FileBrowser.Filter = "ArmaExtensionInterface.csproj |ArmaExtensionInterface.csproj" 
$FileBrowser.InitialDirectory = Get-Location
$null = $FileBrowser.ShowDialog()
$ArmaExtensionInterface = (get-item $FileBrowser.FileName).Directory.FullName

New-Item -Path .\ArmaExtensionInterface -ItemType SymbolicLink -Value $ArmaExtensionInterface
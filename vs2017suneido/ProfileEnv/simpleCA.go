AddFile('profileProgress.txt', '\r\n-------------------------------------------')
AddFile('profileProgress.txt', '\r\nsimpleCA.go: Started')
AddFile('profileProgress.txt', '\r\nUse: Started')
Use('axonlib')
Use('Accountinglib')
Use('etalib')
Use('pcmiler')
Use('prlib')
Use('prcadlib')
Use('etaprlib')
Use('configlib')
AddFile('profileProgress.txt', '\r\nUse: Completed\r\nDemo_Data: Started')
Create_DemoData('CAD')
AddFile('profileProgress.txt', '\r\nDemo_Data: Completed\r\nTest: Started')
TestRunner.Run()
AddFile('profileProgress.txt', '\r\nTest: Completed')
AddFile('profileProgress.txt', '\r\nsimpleCA.go: Completed')
Exit()
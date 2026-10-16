import sys, os
os.environ['ARM32_EXE']=r'D:\repo\Win32Craft-main\Minecraft_1.16.40.02\Minecraft.Windows.exe'
sys.path.insert(0,r'D:\repo\Win32Craft-main\tools\arm32')
import arm32re
arm32re.CACHE = r'D:\repo\Win32Craft-main\arm32\v1_16_40\analysis\cache_appview'
sys.argv[0]='arm32re'
arm32re.main()

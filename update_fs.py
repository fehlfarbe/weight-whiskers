import os
Import("env")

def before_buildfs(source, target, env):
    print("Building react app...")
    os.system("./create_www.sh")

env.AddPreAction("buildfs", before_buildfs)
env.AddPreAction("uploadfs", before_buildfs)
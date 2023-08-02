import os
Import("env")

def before_upload(source, target, env):
    print("Building react app...")
    os.system("./create_www.sh")
    # do some actions

env.AddPreAction("upload", before_upload)
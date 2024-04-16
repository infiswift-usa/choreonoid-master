print("Hello World")
import sys
print(sys.path)

import cnoid.testmodule as tm
print(tm.myname)

# Now you can use the Base module
import cnoid.example as e
print(e.myname)
e.say_hello("Rahul")

from cnoid.example import say_hello
say_hello("Rahul")
from cnoid import Base

# from cnoid.BodyPlugin import *

# simulatorItem = Item.find("AISTSimulator")
# RootItem.instance.selectItem(simulatorItem)
# simulatorItem.startSimulation()
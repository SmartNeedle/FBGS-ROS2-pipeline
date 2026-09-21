from glob import glob
from setuptools import setup

package_name = "ros2_smartneedle_adapter"

setup(
    name=package_name,
    version="0.1.0",
    packages=[package_name],
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
        ("share/" + package_name + "/launch", glob("launch/*.launch.py")),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    entry_points={
        "console_scripts": [
            "smartneedle_igtl_100hz = ros2_smartneedle_adapter.smartneedle_igtl_100hz:main",
        ],
    },
)

import os
from glob import glob
from setuptools import setup

package_name = 'smartneedle_interface'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/files', glob('files/*')),
        ('share/' + package_name + '/launch', glob('launch/*.launch.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Mariana Bernardes',
    maintainer_email='bernardes@unb.br',
    description='Smart Needle OpenIGTLink interface',
    license='TODO: License declaration',
    entry_points={
        'console_scripts': [
            'smartneedle_igtl = smartneedle_interface.smartneedle_igtl:main',
            'virtual_smartneedle = smartneedle_interface.virtual_smartneedle:main',
        ],
    },
)

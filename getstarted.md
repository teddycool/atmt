# How to get started

## Development environment

You will need Python3 and some kind of git-client.

The main software is Visual Studio Code, install it for your platform from https://code.visualstudio.com/

When vscode is up and running, select extensions and search for platformio. This is a complete toolkit for all kind of embedded development. We will use the arduino platform.

We are using C++ and installing some extensions for that is a good idea. 

We will probably use PHP, Python and other stuff as well, and vscode is really good for that too.

Now, when the development environment is installed, clone the github repo.

## How to start working with the repo

(More details for the git-magic in the wiki)

* Create a github-account if you don't already have one
* Navigate to https://github.com/teddycool/atmt
* Click the fork-button to get your own copy of this repo. Use the default and fork only the main-branch.
* Now you have your own copy of the repo in your account and you can start working with it.
* Go the the local directory for your projects and start a terminal
* Run "git clone and the path to your fork to get all the files
* For your own coding you also need to create a new branch and work in that.
* When you are done with your changes, commit and push them to your github-account and create the new feature-branch there.
* Then use a pull-request from your github feature-branch back to the atmt main to show and discuss the changes you made with the rest of the team. 
* Don't forget to sync with the main-branch before you start working on a new feature.

## Open the project in VsCode

Now, start VsCode and open the folder named **EmbeddedSw** in vscode. This contains the PlatformIO project for the autonomous truck firmware.

1. Open Visual Studio Code
2. Navigate to File > Open Folder
3. Select the `EmbeddedSw` folder from your cloned repository
4. PlatformIO should automatically detect the project
5. Use the PlatformIO toolbar to build and upload to your ESP32

For different functionalities, check the `platformio.ini` file which defines multiple environments:
- `control_loop`: Main autonomous control system
- `minimal_test`: Basic ESP32 functionality testing
- `wifi_test`: WiFi connectivity testing

If you have an ESP32 dev-board ready, you should be able to build and upload successfully.
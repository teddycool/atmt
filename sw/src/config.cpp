#include <config.h>;

Config::Config(void)
{
    ID = ESP.getEfuseMac();

    switch (ID)
    {
    case 0xE0DE4C08B764: // PAT03
                         // Here we set the global config variables for this truck
        NAME = "PAT03";
        break;

    default:

        break;
    }
}
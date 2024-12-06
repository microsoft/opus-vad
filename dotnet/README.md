# Opus Voice Activity Detector

Using the freely available opus vad library https://github.com/ms-cleblanc/opus-vad we provide a dotnet wrapper for the library.

To build and update the NuGet package run

dotnet pack --configuration Release
dotnet nuget push bin/Release/OpusVAD.1.3.2.nupkg --api-key <KEY> --source https://api.nuget.org/v3/index.json
# Sandbox

Toy sub-projects are polished here.

## Contents

* CachingMessageQueue: In-memory caching to block writing threads for as short as possible.```TODO(dkorolev): Link once it's pushed.```

* ClientFileStorage: Appends messages to the current file, maintains certain file size and max. age, renames the current file to a finalized one, notifies file listener thread upon newly added finalized files available.

* PosixFileSystem: Thin wrappers over POSIX file system, prep for ClientFileStorage. Tested on Linux and MacOS.
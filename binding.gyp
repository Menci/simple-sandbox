{
  "targets": [
    {
      "target_name": "sandbox",
      "sources": [ "native/sandbox.cc", 
        "native/addon.cc", 
        "native/utils.cc", 
        "native/cgroup.cc", 
        "native/semaphore.cc",
        "native/pipe.cc" ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "libraries": [ 
        "-lboost_filesystem",
        "-lboost_system",
        "-lfuse",
      ],
      "cflags": [
        "-std=c++11",
        "-pthread",
      ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
    }
  ]
}

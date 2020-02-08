{
  "targets": [
    {
      "target_name": "sandbox",
      "sources": [
        "native/sandbox.cc",
        "native/addon.cc",
        "native/utils.cc",
        "native/cgroup.cc",
        "native/semaphore.cc",
        "native/pipe.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "libraries": [
        "-lstdc++fs",
        "-lfmt"
      ],
      "cflags_cc": [
        "-std=c++17",
        "-pthread",
        "-Wno-deprecated-declarations"
      ],
      "cflags_cc!": [
        "-fno-exceptions",
        "-std=gnu++1y"
      ]
    }
  ]
}

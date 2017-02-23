solution 'coyote'
    configurations {'Release', 'Debug'}
    location 'build'

    newaction {
        trigger = "install",
        description = "Install the library",
        execute = function ()
            os.copyfile('source/coyote.hpp', '/usr/local/include/coyote.hpp')

            print(string.char(27) .. '[32mCoyote library installed.' .. string.char(27) .. '[0m')
            os.exit()
        end
    }

    newaction {
        trigger = 'uninstall',
        description = 'Remove all the files installed during build processes',
        execute = function ()
            os.execute('rm -f /usr/local/include/coyote.hpp')
            print(string.char(27) .. '[32mCoyote library uninstalled.' .. string.char(27) .. '[0m')
            os.exit()
        end
    }

    project 'coyoteTest'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/coyote.hpp', 'test/**.hpp', 'test/**.cpp'}

        -- Define the include paths
        includedirs {'/usr/local/include'}
        libdirs {'/usr/local/lib'}

        -- Link the dependencies
        links {'usb-1.0'}

        -- Declare the configurations
        configuration 'Release'
            targetdir 'build/Release'
            defines {'NDEBUG'}
            flags {'OptimizeSpeed'}
        configuration 'Debug'
            targetdir 'build/Debug'
            defines {'DEBUG'}
            flags {'Symbols'}

        -- Linux specific settings
        configuration 'linux'
            buildoptions {'-std=c++11'}
            linkoptions {'-std=c++11'}
            postbuildcommands {'cp ../source/coyote.hpp /usr/local/include/coyote.hpp'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}
            postbuildcommands {'cp ../source/coyote.hpp /usr/local/include/coyote.hpp'}

    project 'changeId'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/changeId.cpp'}

        -- Define the include paths
        includedirs {'/usr/local/include'}
        libdirs {'/usr/local/lib'}

        -- Link the dependencies
        links {'usb-1.0'}

        -- Declare the configurations
        configuration 'Release'
            targetdir 'build/Release'
            defines {'NDEBUG'}
            flags {'OptimizeSpeed'}
        configuration 'Debug'
            targetdir 'build/Debug'
            defines {'DEBUG'}
            flags {'Symbols'}

        -- Linux specific settings
        configuration 'linux'
            buildoptions {'-std=c++11'}
            linkoptions {'-std=c++11'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}

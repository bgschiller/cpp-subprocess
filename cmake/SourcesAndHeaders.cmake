set(sources
    src/ExitStatus.cpp
)

set(exe_sources
		${sources}
)

set(headers
    include/subprocess/CaptureData.hpp
    include/subprocess/ExitStatus.hpp
    include/subprocess/Popen.hpp
    include/subprocess/PopenConfig.hpp
    include/subprocess/RaggedCstrArray.hpp
)

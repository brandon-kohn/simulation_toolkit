#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/system/error_code.hpp>
#include <iostream>
#include <boost/dll.hpp>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <regex>

int main(int argc, char* argv[])
{
	if (argc < 2)
    {
        std::cerr << "Usage: gtest_runner <path-to-dll> [-t=<timeout in milliseconds>] [google test options]" << std::endl;
        return 0;
    }

    int timeout = 60000;
	std::string fname = "RUN_GOOGLE_TESTS";
	if (argc > 2)
	{
		for (auto i = 2; i < argc; ++i) 
		{
			std::regex timeoutRegex("\\s*-t=(\\d+)");
			std::cmatch match;
			if (std::regex_match(argv[i], match, timeoutRegex)) 
			{
				try 
				{
					timeout = boost::lexical_cast<int>(match.str(1));
				}
				catch (...)
				{
					std::cerr << "Bad format specified for timeout option.\n" << "Usage: gtest_runner <path-to-module> [-t=<timeout in milliseconds>] [-fname=\"MyTestHookFunction\"] [google test options]" << std::endl;
					return 1;
				}
				continue;
			}

			std::regex fnameRegex("\\s*-fname=(\\w+)");
			if (std::regex_match(argv[i], match, fnameRegex)) 
			{
				try 
				{
					fname = match.str(1);
				}
				catch (...)
				{
					std::cerr << "Bad format specified for fname option.\n" << "Usage: gtest_runner <path-to-module> [-t=<timeout in milliseconds>] [-fname=MyTestHookFunction] [google test options]" << std::endl;
					return 1;
				}
				continue;
			}
		}
	}

    std::string dllpath = argv[1];

    int result = 1;

    try
    {
        auto tests = boost::dll::import<int(int*, char**)>(dllpath, fname.c_str(), boost::dll::load_mode::append_decorations);

        std::cout << "Running Tests in " << dllpath << std::endl;
        boost::chrono::system_clock::time_point deadline = boost::chrono::system_clock::now() + boost::chrono::milliseconds(timeout);
        boost::thread testThread([&](){ result = tests(&argc, argv); });
        testThread.try_join_until(deadline);
        if (boost::chrono::system_clock::now() > deadline)
        {
            std::cout << "Warning: tests may have exceeded timeout of " << timeout << " ms. This may indicate a test with an infinite loop.\nTry running again with a longer timeout using the -t option." << std::endl;
        }
    }
    catch(...)
    {
        std::cerr << "Error: Cannot load module at: " << dllpath << std::endl;
    }
    
    return result;
}

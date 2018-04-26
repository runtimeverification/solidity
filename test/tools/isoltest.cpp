/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <libdevcore/CommonIO.h>
#include <test/libsolidity/AnalysisFramework.h>
#include <test/libsolidity/SyntaxTest.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <queue>

using namespace dev;
using namespace dev::solidity;
using namespace dev::solidity::test;
using namespace dev::solidity::test::formatting;
using namespace std;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

struct SyntaxTestStats
{
	int successCount;
	int runCount;
	operator bool() const { return successCount == runCount; }
};

class SyntaxTestTool
{
public:
	SyntaxTestTool(string const& _name, fs::path const& _path, bool _formatted):
		m_formatted(_formatted), m_name(_name), m_path(_path)
	{}

	enum class Result
	{
		Success,
		Failure,
		Exception
	};

	Result process();

	static SyntaxTestStats processPath(
		fs::path const& _basepath,
		fs::path const& _path,
		bool const _formatted
	);

	static string editor;
private:
	enum class Request
	{
		Skip,
		Rerun,
		Quit
	};

	Request handleResponse(bool const _exception);

	void printContract() const;

	bool const m_formatted;
	string const m_name;
	fs::path const m_path;
	unique_ptr<SyntaxTest> m_test;
};

string SyntaxTestTool::editor;

void SyntaxTestTool::printContract() const
{
	if (m_formatted)
	{
		string const& source = m_test->source();
		if (source.empty())
			return;

		std::vector<char const*> sourceFormatting(source.length(), formatting::RESET);
		for (auto const& error: m_test->errorList())
			if (error.locationStart >= 0 && error.locationEnd >= 0)
			{
				assert(static_cast<size_t>(error.locationStart) < source.length());
				assert(static_cast<size_t>(error.locationEnd) < source.length());
				bool isWarning = error.type == "Warning";
				for (int i = error.locationStart; i < error.locationEnd; i++)
					if (isWarning)
					{
						if (sourceFormatting[i] == formatting::RESET)
							sourceFormatting[i] = formatting::ORANGE_BACKGROUND;
					}
					else
						sourceFormatting[i] = formatting::RED_BACKGROUND;
			}

		cout << "    " << sourceFormatting.front() << source.front();
		for (size_t i = 1; i < source.length(); i++)
		{
			if (sourceFormatting[i] != sourceFormatting[i - 1])
				cout << sourceFormatting[i];
			if (source[i] != '\n')
				cout << source[i];
			else
			{
				cout << formatting::RESET << endl;
				if (i + 1 < source.length())
					cout << "    " << sourceFormatting[i];
			}
		}
		cout << formatting::RESET << endl;
	}
	else
	{
		stringstream stream(m_test->source());
		string line;
		while (getline(stream, line))
			cout << "    " << line << endl;
		cout << endl;
	}
}

SyntaxTestTool::Result SyntaxTestTool::process()
{
	bool success;
	std::stringstream outputMessages;

	(FormattedScope(cout, m_formatted, {BOLD}) << m_name << ": ").flush();

	try
	{
		m_test = unique_ptr<SyntaxTest>(new SyntaxTest(m_path.string()));
		success = m_test->run(outputMessages, "  ", m_formatted);
	}
	catch(CompilerError const& _e)
	{
		FormattedScope(cout, m_formatted, {BOLD, RED}) <<
			"Exception: " << SyntaxTest::errorMessage(_e) << endl;
		return Result::Exception;
	}
	catch(InternalCompilerError const& _e)
	{
		FormattedScope(cout, m_formatted, {BOLD, RED}) <<
			"InternalCompilerError: " << SyntaxTest::errorMessage(_e) << endl;
		return Result::Exception;
	}
	catch(FatalError const& _e)
	{
		FormattedScope(cout, m_formatted, {BOLD, RED}) <<
			"FatalError: " << SyntaxTest::errorMessage(_e) << endl;
		return Result::Exception;
	}
	catch(UnimplementedFeatureError const& _e)
	{
		FormattedScope(cout, m_formatted, {BOLD, RED}) <<
			"UnimplementedFeatureError: " << SyntaxTest::errorMessage(_e) << endl;
		return Result::Exception;
	}
	catch (std::exception const& _e)
	{
		FormattedScope(cout, m_formatted, {BOLD, RED}) << "Exception: " << _e.what() << endl;
		return Result::Exception;
	}
	catch(...)
	{
		FormattedScope(cout, m_formatted, {BOLD, RED}) <<
			"Unknown Exception" << endl;
		return Result::Exception;
	}

	if (success)
	{
		FormattedScope(cout, m_formatted, {BOLD, GREEN}) << "OK" << endl;
		return Result::Success;
	}
	else
	{
		FormattedScope(cout, m_formatted, {BOLD, RED}) << "FAIL" << endl;

		FormattedScope(cout, m_formatted, {BOLD, CYAN}) << "  Contract:" << endl;
		printContract();

		cout << outputMessages.str() << endl;
		return Result::Failure;
	}
}

SyntaxTestTool::Request SyntaxTestTool::handleResponse(bool const _exception)
{
	if (_exception)
		cout << "(e)dit/(s)kip/(q)uit? ";
	else
		cout << "(e)dit/(u)pdate expectations/(s)kip/(q)uit? ";
	cout.flush();

	while (true)
	{
		switch(readStandardInputChar())
		{
		case 's':
			cout << endl;
			return Request::Skip;
		case 'u':
			if (_exception)
				break;
			else
			{
				cout << endl;
				ofstream file(m_path.string(), ios::trunc);
				file << m_test->source();
				file << "// ----" << endl;
				if (!m_test->errorList().empty())
					m_test->printErrorList(file, m_test->errorList(), "// ", false);
				return Request::Rerun;
			}
		case 'e':
			cout << endl << endl;
			if (system((editor + " \"" + m_path.string() + "\"").c_str()))
				cerr << "Error running editor command." << endl << endl;
			return Request::Rerun;
		case 'q':
			cout << endl;
			return Request::Quit;
		default:
			break;
		}
	}
}


SyntaxTestStats SyntaxTestTool::processPath(
	fs::path const& _basepath,
	fs::path const& _path,
	bool const _formatted
)
{
	std::queue<fs::path> paths;
	paths.push(_path);
	int successCount = 0;
	int runCount = 0;

	while (!paths.empty())
	{
		auto currentPath = paths.front();

		fs::path fullpath = _basepath / currentPath;
		if (fs::is_directory(fullpath))
		{
			paths.pop();
			for (auto const& entry: boost::iterator_range<fs::directory_iterator>(
				fs::directory_iterator(fullpath),
				fs::directory_iterator()
			))
				if (fs::is_directory(entry.path()) || SyntaxTest::isTestFilename(entry.path().filename()))
					paths.push(currentPath / entry.path().filename());
		}
		else
		{
			SyntaxTestTool testTool(currentPath.string(), fullpath, _formatted);
			++runCount;
			auto result = testTool.process();

			switch(result)
			{
			case Result::Failure:
			case Result::Exception:
				switch(testTool.handleResponse(result == Result::Exception))
				{
				case Request::Quit:
					return { successCount, runCount };
				case Request::Rerun:
					cout << "Re-running test case..." << endl;
					--runCount;
					break;
				case Request::Skip:
					paths.pop();
					break;
				}
				break;
			case Result::Success:
				paths.pop();
				++successCount;
				break;
			}
		}
	}

	return { successCount, runCount };

}

int main(int argc, char *argv[])
{
	if (getenv("EDITOR"))
		SyntaxTestTool::editor = getenv("EDITOR");
	else if (fs::exists("/usr/bin/editor"))
		SyntaxTestTool::editor = "/usr/bin/editor";

	fs::path testPath;
	bool formatted = true;
	po::options_description options(
		R"(isoltest, tool for interactively managing test contracts.
Usage: isoltest [Options] --testpath path
Interactively validates test contracts.

Allowed options)",
		po::options_description::m_default_line_length,
		po::options_description::m_default_line_length - 23);
	options.add_options()
		("help", "Show this help screen.")
		("testpath", po::value<fs::path>(&testPath), "path to test files")
		("no-color", "don't use colors")
		("editor", po::value<string>(&SyntaxTestTool::editor), "editor for opening contracts");

	po::variables_map arguments;
	try
	{
		po::command_line_parser cmdLineParser(argc, argv);
		cmdLineParser.options(options);
		po::store(cmdLineParser.run(), arguments);

		if (arguments.count("help"))
		{
			cout << options << endl;
			return 0;
		}

		if (arguments.count("no-color"))
			formatted = false;

		po::notify(arguments);
	}
	catch (po::error const& _exception)
	{
		cerr << _exception.what() << endl;
		return 1;
	}

	if (testPath.empty())
	{
		auto const searchPath =
		{
			fs::current_path() / ".." / ".." / ".." / "test",
			fs::current_path() / ".." / ".." / "test",
			fs::current_path() / ".." / "test",
			fs::current_path() / "test",
			fs::current_path()
		};
		for (auto const& basePath : searchPath)
		{
			fs::path syntaxTestPath = basePath / "libsolidity" / "syntaxTests";
			if (fs::exists(syntaxTestPath) && fs::is_directory(syntaxTestPath))
			{
				testPath = basePath;
				break;
			}
		}
	}

	fs::path syntaxTestPath = testPath / "libsolidity" / "syntaxTests";

	if (fs::exists(syntaxTestPath) && fs::is_directory(syntaxTestPath))
	{
		auto stats = SyntaxTestTool::processPath(testPath / "libsolidity", "syntaxTests", formatted);

		cout << endl << "Summary: ";
		FormattedScope(cout, formatted, {BOLD, stats ? GREEN : RED}) <<
			stats.successCount << "/" << stats.runCount;
		cout << " tests successful." << endl;

		return stats ? 0 : 1;
	}
	else
	{
		cerr << "Test path not found. Use the --testpath argument." << endl;
		return 1;
	}
}

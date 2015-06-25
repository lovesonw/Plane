#include <Book/Application.hpp>

#include <stdexcept>
#include <iostream>


int main()
{
	try
	{
		Application app;
		app.run();
	}
	catch (std::exception& error)
	{
		std::cout << "\nEXCEPTION: " << error.what() << std::endl;
	}
	return 0;
}

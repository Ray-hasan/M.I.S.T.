#include "imgui/imgui.h"
#include "imgui/imgui-SFML.h"
#include <iostream>
#include <string>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <SFML/system.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Window.hpp>
#include <functional>
#include <cstdint>
#include <fstream>
#include <vector>
#include <filesystem>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>




struct Measurement {
	std::string type;
	double value;
};

void loadFont(sf::Font& font, std::string str) {
	if (!font.openFromFile(str)) {
		std::cerr << "Error loading font" << std::endl;
	}
}

int fileCount = 0;

int countFiles(const std::string& path) {

	namespace fs = std::filesystem;
	int count = 0;

	try {
		for (const auto& entry : fs::directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".mis") {
				++count;
			}
			
		}
	}
	catch (const fs::filesystem_error& e) {
		std::cerr << "Error counting files: " << e.what() << std::endl;
	}

	return count;

}

void saveFile(const std::string& filename, const std::vector<Measurement>& measurements) {

	std::ofstream file(filename, std::ios::out | std::ios::binary);
	if (!file) {
		std::cerr << "Error opening file for writing: " << filename << std::endl;
		return;
	}

	for (const auto& measurement : measurements) {
		file << measurement.type << " " << measurement.value << "\n";
	}

	file.close();

	fileCount++;
}

/*This code needs to be edited. I want it to return an array of all of the measurements and all of the 
contents in the file including measurements */

/*
make sure the number of files gets added to an integer counter every time a new file is saved.
*/

std::vector<Measurement> loadFile(const std::string& filename) {
	std::vector<Measurement> measurements;
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	if (!file) {
		std::cerr << "Error opening file for reading: " << filename << std::endl;
		return measurements;
	}

	Measurement measurement;
	while (file >> measurement.type >> measurement.value) {
		measurements.push_back(measurement);
	}

	file.close();
	return measurements;
}
	

class textBox {
public:

	sf::RectangleShape textBoxBox;
	sf::Text placeholder;
	sf::Text userIn;
	sf::RectangleShape cursor;
	sf::Clock cursorBlink;
	std::string input;
	sf::FloatRect textVisibleArea;
	float scrollOffset = 0.f;
	float maxTextWidth = 0.f;
	bool isSelected = false;
	bool enterPressed = false;
	bool cursorVisible = false;
	std::size_t cursorPosition = 0;
	sf::Vector2f textPos;


	textBox(const sf::Vector2f& size, const std::string& placeholderText, const sf::Font& font, const sf::Vector2f& boxPos,
		const sf::Vector2f& textPos, unsigned int charSize) : placeholder(font), userIn(font), textPos(textPos) {

		textBoxBox.setSize(size);
		textBoxBox.setPosition(boxPos);
		textBoxBox.setFillColor(sf::Color(0x0F1117FF));

		placeholder.setString(placeholderText);
		placeholder.setPosition(textPos);
		placeholder.setFillColor(sf::Color(0x484A4EFF));
		placeholder.setCharacterSize(charSize);

		userIn.setPosition(textPos);
		userIn.setFillColor(sf::Color(0xF3F3F3FF));
		userIn.setCharacterSize(charSize);

		cursor.setSize({ 2.f, static_cast<float>(userIn.getCharacterSize()) });
		cursor.setFillColor(sf::Color::White);
		UpdateCursorPosition();

	}

	void UpdateCursorPosition() {
		if (input.empty()) {
			cursor.setPosition({ textPos.x + 5.f - scrollOffset, textPos.y });
		}
		else {
			sf::Vector2f textEnd = userIn.findCharacterPos(cursorPosition);
			cursor.setPosition({ std::min(textEnd.x, textBoxBox.getPosition().x + textBoxBox.getSize().x - 5.f), textEnd.y });
		}
	}

	void UpdateCursor() {

		if (isSelected) {
			// Toggle visibility every 0.5 seconds
			if (cursorBlink.getElapsedTime().asSeconds() > 0.5f) {
				cursorVisible = !cursorVisible;
				cursorBlink.restart();
			}
		}
		else {
			cursorVisible = false;
		}
	}

	void UpdateTextPosition() {
		// Calculate text bounds
		float textWidth = userIn.getLocalBounds().size.x;
		float boxWidth = textBoxBox.getSize().x - 10.f; // Account for padding

		// Update scroll offset
		if (textWidth > boxWidth) {
			maxTextWidth = textWidth;
			scrollOffset = std::max(textWidth - boxWidth, scrollOffset);
		}
		else {
			scrollOffset = 0.f;
		}

		// Update text position
		userIn.setPosition({ textPos.x + 5.f - scrollOffset, textPos.y });

		// Update visible area for clipping
		textVisibleArea = sf::FloatRect({ textBoxBox.getPosition().x + 2.f, textBoxBox.getPosition().y + 2.f },
			{ textBoxBox.getSize().x - 4.f, textBoxBox.getSize().y - 4.f });
	}


	void eventHandler(const std::optional<sf::Event>& event, const sf::RenderWindow* window) {

		sf::Vector2f mousePos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));
		if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {

			isSelected = textBoxBox.getGlobalBounds().contains({ mousePos.x, mousePos.y });
			if (isSelected) {
				textBoxBox.setOutlineColor(sf::Color(0x1994FFFF));
				textBoxBox.setOutlineThickness(1.0f);
			}
			else {
				textBoxBox.setOutlineThickness(0.0f);
			}

		}
		if (isSelected) {
			if (const auto* textEvent = event->getIf<sf::Event::TextEntered>()) {
				if ((sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace) || textEvent->unicode == '\b') && !input.empty()) {
					input.pop_back();
					cursorPosition = std::max(cursorPosition - 1, static_cast<std::size_t>(0));
					scrollOffset = std::max(0.f, scrollOffset - userIn.getCharacterSize());

				}
				else if (textEvent->unicode == '\r' || textEvent->unicode == '\n') {
					input.insert(cursorPosition, 1, '\n');
					cursorPosition++;
				}

				else if (textEvent->unicode < 128 && textEvent->unicode != 27) {
					input.insert(cursorPosition, 1, static_cast<char>(textEvent->unicode));
					cursorPosition++;

				}
				
				userIn.setString(input);
				UpdateTextPosition();
				UpdateCursorPosition();
				cursorVisible = true;
				cursorBlink.restart();

			}
			if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
				if (keyEvent->code == sf::Keyboard::Key::Enter) {
					enterPressed = true;
				}
			}
		}
	}

	void draw(sf::RenderWindow* window) {

		window->draw(textBoxBox);
		if (input.empty() && !isSelected) {
			placeholder.setPosition({ textPos.x + 5.f, textPos.y });
			window->draw(placeholder);
		}
		else {
			window->draw(userIn);
			if (cursorVisible) {
				window->draw(cursor);
			}
		}
	}

	std::string getText() const {
		return input;
	}

	void setText(const std::string& str) {
		input = str;
		userIn.setString(str);
	}

	bool isEnterPressed() {
		bool temp = enterPressed;
		enterPressed = false;
		return temp;
	}

	bool isMouseOver(const sf::RenderWindow* window) const {

		sf::Vector2f mousePos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));
		return textBoxBox.getGlobalBounds().contains({ mousePos.x, mousePos.y });

	}

	void update() {
		UpdateCursor();
	}

};


class Button {
public:

	sf::RectangleShape buttonBox;
	sf::Text buttonText;
	sf::Color stdCol;
	sf::Color hoverCol;

	Button(const sf::Vector2f& size, const std::string& text, const sf::Font& font, 
			const sf::Color& boxCol, const sf::Color& textCol, const sf::Vector2f& Boxpos, 
			const sf::Vector2f& Textpos, unsigned int Charsize) : buttonText(font), stdCol(boxCol), hoverCol(boxCol) {

		buttonBox.setSize(size);
		buttonBox.setOrigin(buttonBox.getSize() / 2.0f);
		buttonBox.setPosition(Boxpos);
		buttonBox.setFillColor(stdCol);

		buttonText.setString(text);
		buttonText.setOrigin(buttonText.getGlobalBounds().size / 2.0f);
		buttonText.setPosition(Textpos);
		buttonText.setFillColor(textCol); 
		buttonText.setCharacterSize(Charsize);
	}

	void hover(const sf::RenderWindow* window) {
		if (isMouseOver(window)) {
			buttonBox.setFillColor(hoverCol);
		}
		else {
			buttonBox.setFillColor(stdCol);
		}
	}

	//Include a dropdown menu method maybe even a class idk ill think ab it

	bool isMouseOver(const sf::RenderWindow* window) const {
		
		sf::Vector2f mousePos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));
		return buttonBox.getGlobalBounds().contains({mousePos.x, mousePos.y});
		
	}

	void draw(sf::RenderWindow* window) {
		window->draw(buttonBox);
		window->draw(buttonText);
	}

};


//class dropletRow {
//public:
//
//	dropletRow(textBox& desc(), 
//				textBox& initMeasurement(), 
//				Button& initUnit(),	
//				sf::Sprite& sprite, 
//				textBox& endMeasurement(), 
//				Button& finalUnit()) {}
//
//};


class Dropdown {
public:
	std::vector<Button> items;
	bool isVisible = false;

	Dropdown(const std::vector<std::string>& itemTexts, const sf::Font& font, const sf::Vector2f& size, const sf::Vector2f& position, unsigned int charSize) {
	
		for (size_t i = 0; i < itemTexts.size(); ++i)
		{
			Button item({ size.x, size.y }, itemTexts[i], font, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { position.x, position.y + i * (size.y + 5) }, {position.x, position.y + i * (size.y + 5)}, charSize);
			item.hoverCol = sf::Color(0x0F1117FF);
			items.push_back(item);
		}
	
	}
	void draw(sf::RenderWindow* window) {
		if (isVisible) {
			for (auto& item : items) {
				item.draw(window);
			}
		}
	}

	void hover(const sf::RenderWindow* window) {
		if (isVisible) {
			for (auto& item : items) {
				item.hover(window);
			}
		}
	}

};

int main() {


	sf::RenderWindow* window = new sf::RenderWindow(sf::VideoMode::getDesktopMode(), "M.I.S.T.", sf::State::Fullscreen);

	window->setFramerateLimit(60);

	bool exitBox = false;
	bool mainMenu = true;
	bool createFile = false;
	bool projectScreen = false;

	sf::Font bodyFont;
	loadFont(bodyFont, "Fonts/Kanit-Medium.ttf");
	sf::Text bodyText(bodyFont);

	sf::Font titleFont;
	loadFont(titleFont, "Fonts/Kanit-Bold.ttf");
	sf::Text titleText(titleFont);

	sf::Texture arrowIcon("Assets/Arrow.png", false, sf::IntRect({0, 0} ,{75, 35}));
	sf::Sprite arrow(arrowIcon);
	arrow.setPosition({938 , 456});

	bool imgLoaded = false;


	std::string folderPath = "Files";

	fileCount = countFiles(folderPath);

	const auto mainCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Arrow).value();
	const auto textCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Text).value();




	//Exit Box Setup Begin
	const sf::Vector2u windowSize = window->getSize();
	sf::RectangleShape exit({ 600,200 });
	exit.setOrigin(exit.getSize() / 2.0f);
	exit.setPosition({ windowSize.x / 2.0f, windowSize.y / 2.0f });
	exit.setFillColor(sf::Color(0x17191FFF));

	sf::Text exitText(bodyFont);
	sf::Text Ytext(bodyFont), Ntext(bodyFont);

	exitText.setString("Are you sure you want to exit?");
	exitText.setOrigin(exitText.getGlobalBounds().size / 2.0f);
	exitText.setPosition({ exit.getPosition().x, exit.getPosition().y - 65 });

	Button Ybutton({ 120u,50u }, "Yes", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { exit.getPosition().x - 80, exit.getPosition().y + 50 }, { exit.getPosition().x - 80, exit.getPosition().y + 40 }, 30u);
	Ybutton.hoverCol = sf::Color(0x0F1117FF);
	Button Nbutton({ 120u,50u }, "No", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { exit.getPosition().x + 80, exit.getPosition().y + 50 }, { exit.getPosition().x + 80, exit.getPosition().y + 40 }, 30u);
	Nbutton.hoverCol = sf::Color(0x0F1117FF);

	//Exit Box Setup End


	//Projects Menu Begin

	// The code here will contain a photoshop style layout

	sf::RectangleShape projects({ 1210u, 740u });
	projects.setOrigin(projects.getSize() / 2.0f);
	projects.setPosition({ windowSize.x / 2.0f, windowSize.y / 2.0f });
	projects.setFillColor(sf::Color(0x17191FFF));

	sf::RectangleShape tiles({ 453u, 704u });
	tiles.setOrigin(tiles.getSize() / 2.0f);
	tiles.setPosition({ projects.getPosition().x + 360, projects.getPosition().y });
	tiles.setFillColor(sf::Color(0x0F1117FF));

	Button newFile({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x - 111, tiles.getPosition().y - 260 }, { tiles.getPosition().x - 153, tiles.getPosition().y - 401 }, 200u);
	newFile.hoverCol = sf::Color(0xF3F3F37F);

	Button FilePos1({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x + 111, tiles.getPosition().y - 260 }, { tiles.getPosition().x + 67, tiles.getPosition().y - 401 }, 200u);
	FilePos1.hoverCol = sf::Color(0xF3F3F37F);

	Button FilePos2({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x - 111, tiles.getPosition().y - 86 }, { tiles.getPosition().x - 153, tiles.getPosition().y - 223 }, 200u);
	FilePos2.hoverCol = sf::Color(0xF3F3F37F);

	Button FilePos3({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x + 111, tiles.getPosition().y - 86 }, { tiles.getPosition().x + 67, tiles.getPosition().y - 223 }, 200u);
	FilePos3.hoverCol = sf::Color(0xF3F3F37F);

	Button FilePos4({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x - 111, tiles.getPosition().y + 88 }, { tiles.getPosition().x - 153, tiles.getPosition().y - 53 }, 200u);
	FilePos4.hoverCol = sf::Color(0xF3F3F37F);

	Button FilePos5({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x + 111, tiles.getPosition().y + 88 }, { tiles.getPosition().x + 67, tiles.getPosition().y - 53 }, 200u);
	FilePos5.hoverCol = sf::Color(0xF3F3F37F);

	Button FilePos6({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x - 111, tiles.getPosition().y + 261 }, { tiles.getPosition().x - 153, tiles.getPosition().y + 118 }, 200u);
	FilePos6.hoverCol = sf::Color(0xF3F3F37F);

	Button FilePos7({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x + 111, tiles.getPosition().y + 261 }, { tiles.getPosition().x + 67, tiles.getPosition().y + 118 }, 200u);
	FilePos7.hoverCol = sf::Color(0xF3F3F37F);

	//Insert code that makes it so that clicking on either one of them makes you create a new file. Also, insert code that only loads the new file tiles depending on the number of files present
	titleText.setString("M.I.S.T.");
	titleText.setOrigin(titleText.getGlobalBounds().size / 2.0f);
	titleText.setPosition({ projects.getPosition().x - 455, projects.getPosition().y - 340 });
	titleText.setFillColor(sf::Color(0xF3F3F3FF));
	titleText.setCharacterSize(180u);

	bodyText.setString("Welcome to M.I.S.T., your \n   length measurement \n        storage system.");
	bodyText.setOrigin(bodyText.getGlobalBounds().size / 2.0f);
	bodyText.setPosition({ projects.getPosition().x - 300, projects.getPosition().y - 10 });
	bodyText.setFillColor(sf::Color(0xF3F3F3FF));
	bodyText.setCharacterSize(40u);

	//Projects Menu End





	//New File Window Begin

	// The code here will contain the window that will pop up when a new file is created
	sf::RectangleShape newFileWindow({ 1210u, 740u });
	newFileWindow.setOrigin(newFileWindow.getSize() / 2.0f);
	newFileWindow.setPosition({ windowSize.x / 2.0f, windowSize.y / 2.0f });
	newFileWindow.setFillColor(sf::Color(0x17191FFF));

	//textBox text({L, W}, "Placeholder Text", bodyFont, box{x, y}, text{x, y}, charSize);
	textBox projectName({ 660, 66 }, "Project Name...", bodyFont, { 492, 446 }, { 517, 460 }, 30u);

	textBox fileLoc({ 660, 66 }, "File location...", bodyFont, { 492, 553 }, { 517, 566 }, 30u);

	Button launch({ 385, 95 }, "Launch Project", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { windowSize.x / 2.0f, windowSize.y / 2.0f + 300 }, { windowSize.x / 2.0f - 40, windowSize.y / 2.0f + 290 }, 40u);
	launch.hoverCol = sf::Color(0x0F3F3F37F);

	Button backButton({ 100u,100u }, "<-", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { 292, 280 }, { 292, 270 }, 30u);
	backButton.hoverCol = sf::Color(0x0F1117FF);

	Button browse({150,66}, "Browse", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { 1242, 586 }, { 1242, 575 }, 30u);
	browse.hoverCol = sf::Color(0x0F3F3F37F);

	OPENFILENAMEW openMIS;
	wchar_t szFileMIS[260] = { 0 };
	ZeroMemory(&openMIS, sizeof(openMIS));
	openMIS.lStructSize = sizeof(openMIS);
	openMIS.hwndOwner = NULL;
	openMIS.lpstrFile = szFileMIS;
	openMIS.nMaxFile = sizeof(szFileMIS);
	openMIS.lpstrFilter = L"MIS(.mis)\0*.MIS\0";
	openMIS.nFilterIndex = 1;
	openMIS.lpstrFileTitle = NULL;
	openMIS.nMaxFileTitle = 0;
	openMIS.lpstrInitialDir = NULL;
	openMIS.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;


	//New File Window End

	//Instructions Bar Begin

	// The code here will contain the bar that will include the different instructions that can be performed

	sf::RectangleShape toolBar({ 1880u, 60u });
	toolBar.setPosition({ 20, 20 });
	toolBar.setFillColor(sf::Color(0x17191FFF));

	Button fileButton({ 90u, 50u }, "File", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { 50, 25 }, { 50, 25 }, 30u);
	fileButton.hoverCol = sf::Color(0x0F1117FF);

	std::vector<std::string> dropdownItems = { "New", "Open", "Save", "Save As", "Exit" };
	Dropdown fileDropdown(dropdownItems, bodyFont, { 90, 50 }, { 50, 25 }, 30u);

	sf::RectangleShape measureWindow({ 920u, 954u });
	measureWindow.setPosition({ 20, 106 });
	measureWindow.setFillColor(sf::Color(0x1E212AFF));

	sf::RectangleShape imgWindow({ 680, 750 });
	imgWindow.setPosition({ 1214,106 });
	imgWindow.setFillColor(sf::Color(0x1E212AFF));

	Button imgButton({ 650u, 510u }, "Insert Image \n   (650x510)", bodyFont, sf::Color(0x17191FFF), sf::Color(0xF3F3F3FF), { 1554, 377 }, { 1554, 377 }, 30u);
	imgButton.hoverCol = sf::Color(0x0F1117FF);

	textBox imgDesc({ 650u, 170u }, "Image Description...", bodyFont, { 1229, 650 }, { 1229, 650 }, 30u);

	Button newImg({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { 1554, 1000 }, { 1512, 858 }, 200u);
	newImg.hoverCol = sf::Color(0xF3F3F37F);

	OPENFILENAMEW openIMG;
	wchar_t szFile[260] = { 0 };
	ZeroMemory(&openIMG, sizeof(openIMG));
	openIMG.lStructSize = sizeof(openIMG);
	openIMG.hwndOwner = NULL;
	openIMG.lpstrFile = szFile;
	openIMG.nMaxFile = sizeof(szFile);
	openIMG.lpstrFilter = L"PNG(.png)\0*.PNG\0'All Types'\0*.*\0";
	openIMG.nFilterIndex = 1;
	openIMG.lpstrFileTitle = NULL;
	openIMG.nMaxFileTitle = 0;
	openIMG.lpstrInitialDir = NULL;
	openIMG.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	

	//Instruction Bar End


	//

	// 

	//


	sf::Clock deltaClock;
	while (window->isOpen()) { //Main Loop

		Ybutton.hover(window);
		Nbutton.hover(window);
		newFile.hover(window);
		FilePos1.hover(window);
		FilePos2.hover(window);
		FilePos3.hover(window);
		FilePos4.hover(window);
		FilePos5.hover(window);
		FilePos6.hover(window);
		FilePos7.hover(window);
		backButton.hover(window);
		browse.hover(window);
		launch.hover(window);
		imgButton.hover(window);
		newImg.hover(window);
		fileButton.hover(window);
		fileDropdown.hover(window);

		if (fileCount >= 7) {
			newFile.buttonBox.setPosition({ tiles.getPosition().x + 111, tiles.getPosition().y + 261 });
			newFile.buttonText.setPosition({ tiles.getPosition().x + 67, tiles.getPosition().y + 118 });

		}
		else if (fileCount == 6) {
			newFile.buttonBox.setPosition({ tiles.getPosition().x - 111, tiles.getPosition().y + 261 });
			newFile.buttonText.setPosition({ tiles.getPosition().x - 153, tiles.getPosition().y + 118 });
		}
		else if (fileCount == 5) {
			newFile.buttonBox.setPosition({ tiles.getPosition().x + 111, tiles.getPosition().y + 88 });
			newFile.buttonText.setPosition({ tiles.getPosition().x + 67, tiles.getPosition().y - 53 });
		}
		else if (fileCount == 4) {
			newFile.buttonBox.setPosition({ tiles.getPosition().x - 111, tiles.getPosition().y - 86 });
			newFile.buttonText.setPosition({ tiles.getPosition().x - 153, tiles.getPosition().y - 53 });
		}
		else if (fileCount == 3) {
			newFile.buttonBox.setPosition({ tiles.getPosition().x + 111, tiles.getPosition().y - 86 });
			newFile.buttonText.setPosition({ tiles.getPosition().x + 67, tiles.getPosition().y - 223 });
		}
		else if (fileCount == 2) {
			newFile.buttonBox.setPosition({ tiles.getPosition().x - 111, tiles.getPosition().y - 86 });
			newFile.buttonText.setPosition({ tiles.getPosition().x - 153, tiles.getPosition().y - 223 });
		}
		else if (fileCount == 1) {
			newFile.buttonBox.setPosition({ tiles.getPosition().x + 111, tiles.getPosition().y - 260 });
			newFile.buttonText.setPosition({ tiles.getPosition().x + 67, tiles.getPosition().y - 401 });
		}
		else {
			newFile.buttonBox.setPosition({ tiles.getPosition().x - 111, tiles.getPosition().y - 260 });
			newFile.buttonText.setPosition({ tiles.getPosition().x - 153, tiles.getPosition().y - 401 });
		}

		// Exit Box Begin
		while (auto event = window->pollEvent()) {

			projectName.eventHandler(event, window);
			fileLoc.eventHandler(event, window);
			imgDesc.eventHandler(event, window);


			if (event->is<sf::Event::Closed>()) {

				exitBox = true;
			}

			if (event->is<sf::Event::KeyPressed>()) {

				const auto* keyEvent = event->getIf<sf::Event::KeyPressed>();

				if (keyEvent->scancode == sf::Keyboard::Scan::Escape) {

					if (exitBox) {
						exitBox = false;
					}
					else {
						exitBox = true;
					}
				}
			}

			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && exitBox) {

				if (Ybutton.isMouseOver(window)) {
					window->close();
				}
				else if (Nbutton.isMouseOver(window)) {
					exitBox = false;
				}
			}

			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && mainMenu) {

				if (newFile.isMouseOver(window)) {
					mainMenu = false;
					createFile = true;
				}

			}

			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && createFile) {

				if (backButton.isMouseOver(window)) {
					mainMenu = true;
					createFile = false;
				}

				if (launch.isMouseOver(window)) {
					createFile = false;
					mainMenu = false;
					projectScreen = true;
				}

				if (browse.isMouseOver(window)) {
					GetOpenFileName(&openMIS);
					mainMenu = false;
					createFile = true;
				}


				if (projectName.isMouseOver(window)) { window->setMouseCursor(textCursor); }
				else { window->setMouseCursor(mainCursor); }

				if (fileLoc.isMouseOver(window)) { window->setMouseCursor(textCursor); }
				else { window->setMouseCursor(mainCursor); }
			}

			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && projectScreen) {
				if (imgButton.isMouseOver(window)) {
					//Insert code that allows you to insert an image
					/*TCHAR documentsPath[MAX_PATH];
					SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documentsPath);
					ShellExecute(NULL, L"open", documentsPath, NULL, NULL, SW_SHOWDEFAULT);*/

					
					if (GetOpenFileName(&openIMG)) { imgLoaded = true; }
					

				}


				if (newImg.isMouseOver(window)) {
					//Insert code that allows you to add a new image
				}

				if (fileButton.isMouseOver(window)) { fileDropdown.isVisible = true; }
				else { fileDropdown.isVisible = false; }



				if (imgDesc.isMouseOver(window)) { window->setMouseCursor(textCursor); }
				else { window->setMouseCursor(mainCursor); }
			}


		}

		projectName.update();
		fileLoc.update();
		imgDesc.update();

		// Exit Box End



		window->clear(sf::Color(0x27292FFF)); //last two hexadecimal values are for transperency 

		if (mainMenu) {

			window->draw(projects);
			window->draw(bodyText);
			window->draw(titleText);
			window->draw(tiles);
			newFile.draw(window);
			/*FilePos1.draw(window);
			FilePos2.draw(window);
			FilePos3.draw(window);
			FilePos4.draw(window);
			FilePos5.draw(window);
			FilePos6.draw(window);
			FilePos7.draw(window);*/




		}

		else if (createFile) {

			window->draw(newFileWindow);
			projectName.draw(window);
			fileLoc.draw(window);
			browse.draw(window);
			backButton.draw(window);
			launch.draw(window);



		}

		else if (projectScreen) {

			window->draw(toolBar);
			window->draw(measureWindow);
			window->draw(imgWindow);
			
			imgDesc.draw(window);
			newImg.draw(window);
			window->draw(arrow);
			fileButton.draw(window);
			fileDropdown.draw(window);

			if (imgLoaded) {

				sf::Texture imgInput(szFile, false, sf::IntRect({ 0, 0 }, { 650u, 510u }));
				sf::Sprite imgSprite(imgInput);

				imgSprite.setOrigin(imgSprite.getGlobalBounds().size / 2.0f);
				imgSprite.setPosition({ 1554, 377 });
				window->draw(imgSprite);
			}
			else { imgButton.draw(window); }
			

		}

		if (exitBox) {


			sf::RectangleShape overlay;
			overlay.setSize(static_cast<sf::Vector2f>(window->getSize()));
			overlay.setFillColor(sf::Color(0x0000007F));
			window->draw(overlay);
			window->draw(exit);
			window->draw(exitText);
			Ybutton.draw(window);
			Nbutton.draw(window);

		}

		window->display();


	} //End of Main Loop

	delete window;

	return 0;

}
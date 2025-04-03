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
#include<variant>




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
	

//Conversion functions
static double cmToInch(double cm) { return cm / 2.54; }
static double inchToCm(double inch) { return inch * 2.54; }
static double cmToM(double cm) { return cm / 100; }
static double mToCm(double m) { return m * 100; }
static double inchToM(double inch) { return cmToM(inchToCm(inch)); }
static double mToInch(double m) { return cmToInch(mToCm(m)); }
static double inchToFeet(double inch) { return inch / 12; }
static double feetToInch(double feet) { return feet * 12; }
static double cmToFeet(double cm) { return inchToFeet(cmToInch(cm)); }
static double feetToCm(double feet) { return inchToCm(feetToInch(feet)); }
static double mToFeet(double m) { return cmToFeet(mToCm(m)); }
static double feetToM(double feet) { return cmToM(feetToCm(feet)); }

static double convert(double value, int from, int to) {
	if (from == to) { return value; }

	if (from == 0) {
		if (to == 1) { return inchToCm(value); }
		else if (to == 2) { return inchToFeet(value); }
		else if (to == 3) { return inchToM(value); }
	}
	else if (from == 1) {
		if (to == 0) { return cmToInch(value); }
		else if (to == 2) { return cmToFeet(value); }
		else if (to == 3) { return cmToM(value); }
	}
	else if (from == 2) {
		if (to == 0) { return feetToInch(value); }
		else if (to == 1) { return feetToCm(value); }
		else if (to == 3) { return feetToM(value); }
	}
	else if (from == 3) {
		if (to == 0) { return mToInch(value); }
		else if (to == 1) { return mToCm(value); }
		else if (to == 2) { return mToFeet(value); }
	}
	else { return value; }
	
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
		if (input.empty()) { cursor.setPosition({ textPos.x + 5.f - scrollOffset, textPos.y }); }
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
		else { cursorVisible = false; }
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
		else { scrollOffset = 0.f; }

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
			else { textBoxBox.setOutlineThickness(0.0f); }
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
				if (keyEvent->code == sf::Keyboard::Key::Enter) { enterPressed = true; }
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
			if (cursorVisible) { window->draw(cursor); }
		}
	}

	std::string getText() const { return input; }

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

	void update() { UpdateCursor(); }
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
		if (isMouseOver(window)) { buttonBox.setFillColor(hoverCol); }
		else { buttonBox.setFillColor(stdCol); }
	}

	void setTextPosition(const sf::Vector2f& pos) { buttonText.setPosition(pos); }
	void changeText(const std::string& text) { buttonText.setString(text); }
	void reposition(const sf::Vector2f& buttonPos, const sf::Vector2f& textPos) {
		buttonBox.setPosition(buttonPos);
		buttonText.setPosition(textPos);
	}

	bool isMouseOver(const sf::RenderWindow* window) const {
		
		sf::Vector2f mousePos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));
		return buttonBox.getGlobalBounds().contains({mousePos.x, mousePos.y});
		
	}

	void draw(sf::RenderWindow* window) {
		window->draw(buttonBox);
		window->draw(buttonText);
	}

};

class Droplet {
public:

	//init unit box
	sf::RectangleShape initBox;
	sf::Text initButtonText;
	std::vector<Button> initUnitButtons;
	bool isInitSelected = false;
	int initIndex = 0;

	//final unit box
	sf::RectangleShape finalBox;
	sf::Text finalButtonText;
	std::vector<Button> finalUnitButtons;
	bool isFinalSelected = false;
	int finalIndex = 0;

	//IO
	sf::RectangleShape inputBox;
	sf::RectangleShape outputBox;
	sf::Text placeholder;
	sf::Text oph;
	sf::Text userIn;
	sf::Text userOut;
	sf::RectangleShape cursor;
	sf::Clock cursorBlink;
	std::string input;
	sf::FloatRect iTextVisibleArea;
	sf::FloatRect oTextVisibleArea;
	float scrollOffset = 0.f;
	float maxTextWidth = 0.f;
	bool isSelected = false;
	bool enterPressed = false;
	bool cursorVisible = false;
	bool isTail = false;
	std::size_t cursorPosition = 0;
	sf::Vector2f itextPos;
	sf::Vector2f otextPos;

	//general
	sf::Color std = sf::Color(0x2B2F3EFF);
	sf::Color hoverCol = sf::Color(0x0F1117FF);
	sf::Color textCol = sf::Color(0xF3F3F3FF);
	std::vector<std::string> units = { "in.", "cm", "ft.", "m" };
	sf::Vector2f buttonSize = sf::Vector2f(75, 50);
	sf::Vector2f IOSize = sf::Vector2f(145, 50);
	std::string placeholderText = "0.00";
	unsigned int charSize = 20u;

	Droplet(const sf::Font& font, const sf::Vector2f& initPos, const sf::Vector2f& initText, const sf::Vector2f& finalPos, const sf::Vector2f& finalText, 
			const sf::Vector2f& iBox, const sf::Vector2f& iTextPos, const sf::Vector2f& oBox, const sf::Vector2f& oTextPos) 
			: initButtonText(font), finalButtonText(font), placeholder(font), oph(font), userIn(font), userOut(font), itextPos(iTextPos), otextPos(oTextPos){

		initBox.setSize(buttonSize);
		initBox.setOrigin(initBox.getSize() / 2.0f);
		initBox.setPosition(initPos);
		initBox.setFillColor(std);

		initButtonText.setString(units[0]);
		initButtonText.setOrigin(initButtonText.getGlobalBounds().size / 2.0f);
		initButtonText.setPosition(initPos);
		initButtonText.setFillColor(textCol);
		initButtonText.setCharacterSize(20u);

		finalBox.setSize(buttonSize);
		finalBox.setOrigin(finalBox.getSize() / 2.0f);
		finalBox.setPosition(finalPos);
		finalBox.setFillColor(std);

		finalButtonText.setString(units[0]);
		finalButtonText.setOrigin(finalButtonText.getGlobalBounds().size / 2.0f);
		finalButtonText.setPosition(finalPos);
		finalButtonText.setFillColor(textCol);
		finalButtonText.setCharacterSize(20u);

		for (size_t i = 0; i < units.size(); i++)
		{
			Button initUnit(buttonSize, units[i], font, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { initPos.x, initPos.y + (i + 1) * (buttonSize.y) }, { initPos.x, initPos.y + (i + 1) * (buttonSize.y) - 9 }, 20u);
			initUnit.hoverCol = sf::Color(0x0F1117FF);
			initUnitButtons.push_back(initUnit);
		}

		for (size_t j = 0; j < units.size(); j++)
		{
			Button finalUnit(buttonSize, units[j], font, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { finalPos.x, finalPos.y + (j + 1) * (buttonSize.y) }, { finalPos.x, finalPos.y + (j + 1) * (buttonSize.y) - 9 }, 20u);
			finalUnit.hoverCol = sf::Color(0x0F1117FF);
			finalUnitButtons.push_back(finalUnit);
		}

		inputBox.setSize(IOSize);
		inputBox.setPosition(iBox);
		inputBox.setFillColor(sf::Color(0x0F1117FF));

		outputBox.setSize(IOSize);
		outputBox.setPosition(oBox);
		outputBox.setFillColor(sf::Color(0x0F1117FF));

		placeholder.setString(placeholderText);
		placeholder.setPosition(iTextPos);
		placeholder.setFillColor(sf::Color(0x484A4EFF));
		placeholder.setCharacterSize(charSize);

		oph.setString(placeholderText);
		oph.setPosition(oTextPos);
		oph.setFillColor(sf::Color(0x484A4EFF));
		oph.setCharacterSize(charSize);

		userIn.setPosition(iTextPos);
		userIn.setFillColor(sf::Color(0xF3F3F3FF));
		userIn.setCharacterSize(charSize);

		userOut.setPosition(oTextPos);
		userOut.setFillColor(sf::Color(0xF3F3F3FF));
		userOut.setCharacterSize(charSize);

		cursor.setSize({ 2.f, static_cast<float>(userIn.getCharacterSize()) });
		cursor.setFillColor(sf::Color::White);
		UpdateCursorPosition();

	}

	void UpdateCursorPosition() {
		if (input.empty()) { cursor.setPosition({ itextPos.x + 5.f - scrollOffset, itextPos.y }); }
		else {
			sf::Vector2f textEnd = userIn.findCharacterPos(cursorPosition);
			cursor.setPosition({ std::min(textEnd.x, inputBox.getPosition().x + inputBox.getSize().x - 5.f), textEnd.y });
		}
	}

	void UpdateCursor() {

		if (isSelected) {
			if (cursorBlink.getElapsedTime().asSeconds() > 0.5f) {
				cursorVisible = !cursorVisible;
				cursorBlink.restart();
			}
		}
		else { cursorVisible = false; }
	}

	void UpdateTextPosition() {
		// Calculate text bounds
		float textWidth = userIn.getLocalBounds().size.x;
		float boxWidth = inputBox.getSize().x - 10.f; // Account for padding

		// Update scroll offset
		if (textWidth > boxWidth) {
			maxTextWidth = textWidth;
			scrollOffset = std::max(textWidth - boxWidth, scrollOffset);
		}
		else { scrollOffset = 0.f; }

		// Update text position
		userIn.setPosition({ itextPos.x + 5.f - scrollOffset, itextPos.y });
		userOut.setPosition({ otextPos.x + 5.f - scrollOffset, otextPos.y });

		// Update visible area for clipping
		iTextVisibleArea = sf::FloatRect({ inputBox.getPosition().x + 2.f, inputBox.getPosition().y + 2.f },
			{ inputBox.getSize().x - 4.f, inputBox.getSize().y - 4.f });

		oTextVisibleArea = sf::FloatRect({ outputBox.getPosition().x + 2.f, outputBox.getPosition().y + 2.f },
			{ outputBox.getSize().x - 4.f, outputBox.getSize().y - 4.f });
	}

	bool isNumber(char c) { return std::isdigit(c) || c == '.'; }

	void changeInitText(const std::string& text) { initButtonText.setString(text); }
	void changeFinalText(const std::string& text) { finalButtonText.setString(text); }
	void setInitTextPosition(const sf::Vector2f& pos) { initButtonText.setPosition(pos); }
	void setFinalTextPosition(const sf::Vector2f& pos) { finalButtonText.setPosition(pos); }
	void setInitBoxPosition(const sf::Vector2f& pos) { initBox.setPosition(pos); }
	void setFinalBoxPosition(const sf::Vector2f& pos) { finalBox.setPosition(pos); }
	int getInitIndex() { return initIndex; }
	void setInitIndex(int& i) { initIndex = i; }
	int getFinalIndex() { return finalIndex; }
	void setFinalIndex(int& i) { finalIndex = i; }
	void ghost(sf::Sprite& sprite) {
		initBox.setFillColor(sf::Color(0x2B2F3E7F));
		finalBox.setFillColor(sf::Color(0x2B2F3E7F));
		inputBox.setFillColor(sf::Color(0x0F11177F));
		outputBox.setFillColor(sf::Color(0x0F11177F));
		placeholder.setFillColor(sf::Color(0x484A4E7F));
		oph.setFillColor(sf::Color(0x484A4E7F));
		initButtonText.setFillColor(sf::Color(0xF3F3F37F));
		finalButtonText.setFillColor(sf::Color(0xF3F3F37F));
		userIn.setFillColor(sf::Color(0xF3F3F37F));
		userOut.setFillColor(sf::Color(0xF3F3F37F));
		isTail = true;
		sprite.setColor(sf::Color(0xFFFFFF7F));
	}

	std::string format(const std::string& s) const {
		if (s.empty()) { return "0.00"; }
		try {
			double v = std::stod(s);
			std::stringstream ss;
			ss << std::fixed << std::setprecision(2) << v;
			return ss.str();
		}
		catch (const std::exception&) { return "0.00"; }
	}

	std::string conversion(const std::string& s) const {
		if (s.empty()) { return "0.00"; }
		try {
			double v = std::stod(s);
			double converted = convert(v, initIndex, finalIndex);
			std::stringstream ss;
			ss << std::fixed << std::setprecision(2) << converted;
			return ss.str();
		}
		catch (const std::exception&) { return "0.00"; }
	}

	void eventHandler(const std::optional<sf::Event>& event, const sf::RenderWindow* window) {

		sf::Vector2f mousePos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));
		if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
			isSelected = inputBox.getGlobalBounds().contains({ mousePos.x, mousePos.y });
			if (isSelected) {
				bool wasSelected = isSelected;
				inputBox.setOutlineColor(sf::Color(0x1994FFFF));
				inputBox.setOutlineThickness(1.0f);

				if (!wasSelected) {
					cursorVisible = true;
					cursorBlink.restart();
				}
			}
			else { inputBox.setOutlineThickness(0.0f); }
		}

		if (isSelected) {
			if (const auto* textEvent = event->getIf<sf::Event::TextEntered>()) {
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace) || textEvent->unicode == '\b') {
					if (!input.empty() && cursorPosition > 0) {
						input.erase(cursorPosition - 1, 1);
						cursorPosition = std::max(cursorPosition - 1, static_cast<std::size_t>(0));
						scrollOffset = std::max(0.f, scrollOffset - userIn.getCharacterSize());
					}
				}
				else if (textEvent->unicode < 128 && textEvent->unicode != 27) {
					char c = static_cast<char>(textEvent->unicode);
					if (isNumber(c)) {
						input.insert(cursorPosition, 1, c);
						cursorPosition++;
					}
				}

				userIn.setString(format(input));
				UpdateTextPosition();
				UpdateCursorPosition();
				cursorVisible = true;
				cursorBlink.restart();
				userOut.setString(conversion(input));
			}
			if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
				if (keyEvent->code == sf::Keyboard::Key::Enter) {
					enterPressed = true;
				}
			}
		}
	}

	void initButtonHover(const sf::RenderWindow* window) {
		if (!isInitSelected && isMouseOveri(window)) { initBox.setFillColor(hoverCol); }
		else { initBox.setFillColor(std); }
		if (isInitSelected) { for (auto& initUnit : initUnitButtons) { initUnit.hover(window); } }
	}

	void finalButtonHover(const sf::RenderWindow* window) {
		if (!isFinalSelected && isMouseOverf(window)) { finalBox.setFillColor(hoverCol); }
		else { finalBox.setFillColor(std); }
		if (isFinalSelected) { for (auto& finalUnit : finalUnitButtons) { finalUnit.hover(window); } }
	}

	void draw(sf::RenderWindow* window) {

		window->draw(inputBox);
		window->draw(outputBox);
		window->draw(initBox);
		window->draw(initButtonText);
		window->draw(finalBox);
		window->draw(finalButtonText);

		if (input.empty() && !isSelected) {
			placeholder.setPosition({ itextPos.x + 5.f, itextPos.y });
			oph.setPosition({ otextPos.x + 5.f, otextPos.y });
			window->draw(placeholder);
			window->draw(oph);
		}
		else {
			window->draw(userIn);
			window->draw(userOut);
			if (cursorVisible) {
				window->draw(cursor);
			}
		}	
		if (isInitSelected) { for (auto& initUnit : initUnitButtons) { initUnit.draw(window); } }
		if (isFinalSelected) { for (auto& finalUnit : finalUnitButtons) { finalUnit.draw(window); } }
	}


	std::string getText() const { return input; }

	void setText(const std::string& str) {
		input = str;
		userIn.setString(str);
	}

	bool isEnterPressed() {
		bool temp = enterPressed;
		enterPressed = false;
		return temp;
	}

	bool isMouseOverIO(const sf::RenderWindow* window) const {
		sf::Vector2f mousePos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));
		return inputBox.getGlobalBounds().contains({ mousePos.x, mousePos.y });
	}

	bool isMouseOveri(const sf::RenderWindow* window) const {
		sf::Vector2f mousePos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));
		return initBox.getGlobalBounds().contains({ mousePos.x, mousePos.y });
	}

	bool isMouseOverf(const sf::RenderWindow* window) const {
		sf::Vector2f mousePos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));
		return finalBox.getGlobalBounds().contains({ mousePos.x, mousePos.y });
	}

	void update() { UpdateCursor(); }

};

class Dropdown {
public:
	std::vector<Button> items;
	bool isVisible = false;

	Dropdown(const std::vector<std::string>& itemTexts, const sf::Font& font, const sf::Vector2f& size, const sf::Vector2f& position, unsigned int charSize) {

		for (size_t i = 0; i < itemTexts.size(); i++)
		{
			if (itemTexts.size() >= 5) {
				Button item({ 3 * size.x, size.y }, itemTexts[i], font, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { position.x, position.y + i * (size.y) }, { position.x - 90, position.y + i * (size.y) - 9 }, charSize);
				if (itemTexts[i].length() > 4) { item.setTextPosition({ position.x - 75, position.y + i * (size.y) - 9 }); }
				item.hoverCol = sf::Color(0x0F1117FF);
				items.push_back(item);
			}
			else {
				Button item(size, itemTexts[i], font, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { position.x, position.y + i * (size.y) }, { position.x, position.y + i * (size.y) - 9 }, charSize);
				item.hoverCol = sf::Color(0x0F1117FF);
				items.push_back(item);
			}
		}

	}
	void draw(sf::RenderWindow* window) {
		if (isVisible) { for (auto& item : items) { item.draw(window); } }
	}

	void hover(const sf::RenderWindow* window) {
		if (isVisible) { for (auto& item : items) { item.hover(window); } }
	}

};


using dropletRow = std::variant<textBox, Droplet, sf::Sprite>;


int main() {


	sf::RenderWindow* window = new sf::RenderWindow(sf::VideoMode::getDesktopMode(), "M.I.S.T.", sf::State::Fullscreen);

	const float rowSpace = 60.0f;

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

	

	bool imgLoaded = false;
	bool dropletImgLoaded = false;


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

	/*Button FilePos1({ 215u, 167u }, "+", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { tiles.getPosition().x + 111, tiles.getPosition().y - 260 }, { tiles.getPosition().x + 67, tiles.getPosition().y - 401 }, 200u);
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
	FilePos7.hoverCol = sf::Color(0xF3F3F37F);*/

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

	Button fileButton({ 90u, 50u }, "File", bodyFont, sf::Color(0x1E212AFF), sf::Color(0xF3F3F3FF), { 71, 50 }, { 65, 40 }, 30u);
	fileButton.hoverCol = sf::Color(0x0F1117FF);

	std::vector<std::string> dropdownItems = { "New", "Open", "Save", "Save As", "Exit" };
	Dropdown fileDropdown(dropdownItems, bodyFont, { 90, 50 }, { 161, 100 }, 30u);

	sf::Text projectText(bodyFont);
	projectText.setString("Project: ");
	projectText.setPosition({ 130, 30 });

	sf::RectangleShape projectNameBox({ 300, 50 });
	projectNameBox.setPosition({ 240, 25 });
	projectNameBox.setFillColor(sf::Color(0x2B2F3EFF));

	sf::RectangleShape measureWindow({ 920u, 954u });
	measureWindow.setPosition({ 20, 106 });
	measureWindow.setFillColor(sf::Color(0x181D29FF));

	sf::RectangleShape imgWindow({ 680, 750 });
	imgWindow.setPosition({ 1214,106 });
	imgWindow.setFillColor(sf::Color(0x181D29FF));

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

	Button dropletImg({ 223u, 267u }, "Insert Image \n   (223x267)", bodyFont, sf::Color(0x17191FFF), sf::Color(0xF3F3F3FF), { 171, 272 }, { 200, 280 }, 20u);
	dropletImg.hoverCol = sf::Color(0x0F1117FF);

	OPENFILENAMEW openDropletImg;
	wchar_t szFileDroplet[260] = { 0 };
	ZeroMemory(&openDropletImg, sizeof(openDropletImg));
	openDropletImg.lStructSize = sizeof(openDropletImg);
	openDropletImg.hwndOwner = NULL;
	openDropletImg.lpstrFile = szFileDroplet;
	openDropletImg.nMaxFile = sizeof(szFileDroplet);
	openDropletImg.lpstrFilter = L"PNG(.png)\0*.PNG\0'All Types'\0*.*\0";
	openDropletImg.nFilterIndex = 1;
	openDropletImg.lpstrFileTitle = NULL;
	openDropletImg.nMaxFileTitle = 0;
	openDropletImg.lpstrInitialDir = NULL;
	openDropletImg.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	
	textBox dropletTitle({ 591, 63 }, "Droplet name...", bodyFont, { 294, 138 }, { 295, 150 }, 30u);
	textBox dropletDesc({ 591, 189 }, "Additional notes...", bodyFont, { 294, 216 }, { 295, 218 }, 20u);
	std::vector<dropletRow> dropletRows;
	
	textBox desc({ 170, 50 }, "...", bodyFont, { 60, 447 }, { 66, 465 }, 20u);
	sf::Texture ArrowIcon("Assets/Arrow.png", false, sf::IntRect({ 0, 0 }, { 75, 35 }));
	sf::Sprite Arrow(ArrowIcon);
	Arrow.setPosition({ 531, 454 });

	//droplet(font, initPos, initText, finalPos, finalText, iBox, iTextPos, oBox, oTextPos)
	Droplet droplet(bodyFont, { 447, 472 },  {452, 466 },  { 847, 472 },  { 852, 466 }, { 254, 447 }, { 260, 465 }, { 654, 447 }, { 660, 465 });
	textBox ghostDesc({ 170, 50 }, "...", bodyFont, { 60, 507 }, { 66, 525 }, 20u);
	Droplet ghost(bodyFont, { 447, 532 }, { 452, 526 }, { 847, 532 }, { 852, 526 }, { 254, 507 }, { 260, 525 }, { 654, 507 }, { 660, 525 });
	sf::Sprite ghostArrow(ArrowIcon);
	ghostArrow.setPosition({ 531, 514 });
	ghost.ghost(ghostArrow);

	dropletRows.push_back(desc);
	dropletRows.push_back(droplet);
	dropletRows.push_back(Arrow);


	sf::Clock deltaClock;
	while (window->isOpen()) { //Main Loop

		Ybutton.hover(window);
		Nbutton.hover(window);
		newFile.hover(window);
		/*FilePos1.hover(window);
		FilePos2.hover(window);
		FilePos3.hover(window);
		FilePos4.hover(window);
		FilePos5.hover(window);
		FilePos6.hover(window);
		FilePos7.hover(window);*/
		backButton.hover(window);
		browse.hover(window);
		launch.hover(window);
		imgButton.hover(window);
		newImg.hover(window);
		fileButton.hover(window);
		fileDropdown.hover(window);
		dropletImg.hover(window);
		
		for (auto& element : dropletRows) {
			std::visit([&window](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, Droplet>) {
					arg.initButtonHover(window);
					arg.finalButtonHover(window);
				}
				}, element);

		}


		if (fileCount >= 7) { newFile.reposition({ tiles.getPosition().x + 111, tiles.getPosition().y + 261 }, { tiles.getPosition().x + 67, tiles.getPosition().y + 118 }); }
		else if (fileCount == 6) { newFile.reposition({ tiles.getPosition().x - 111, tiles.getPosition().y + 261 }, { tiles.getPosition().x - 153, tiles.getPosition().y + 118 }); }
		else if (fileCount == 5) { newFile.reposition({ tiles.getPosition().x + 111, tiles.getPosition().y + 88 }, { tiles.getPosition().x + 67, tiles.getPosition().y - 53 }); }
		else if (fileCount == 4) { newFile.reposition({ tiles.getPosition().x - 111, tiles.getPosition().y - 86 }, { tiles.getPosition().x - 153, tiles.getPosition().y - 53 }); }
		else if (fileCount == 3) { newFile.reposition({ tiles.getPosition().x + 111, tiles.getPosition().y - 86 }, { tiles.getPosition().x + 67, tiles.getPosition().y - 223 }); }
		else if (fileCount == 2) { newFile.reposition({ tiles.getPosition().x - 111, tiles.getPosition().y - 86 }, { tiles.getPosition().x - 153, tiles.getPosition().y - 223 }); }
		else if (fileCount == 1) { newFile.reposition({ tiles.getPosition().x + 111, tiles.getPosition().y - 260 }, { tiles.getPosition().x + 67, tiles.getPosition().y - 401 }); }
		else { newFile.reposition({ tiles.getPosition().x - 111, tiles.getPosition().y - 260 }, { tiles.getPosition().x - 153, tiles.getPosition().y - 401 }); }

		// Exit Box Begin
		while (auto event = window->pollEvent()) {

			projectName.eventHandler(event, window);
			fileLoc.eventHandler(event, window);
			imgDesc.eventHandler(event, window);
			dropletDesc.eventHandler(event, window);
			dropletTitle.eventHandler(event, window);

			for (auto& element : dropletRows) {
				std::visit([&event, &window](auto&& arg) {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, textBox>) { arg.eventHandler(event, window); }
					else if constexpr (std::is_same_v<T, Droplet>) {
						arg.getInitIndex();
						arg.getFinalIndex();
						arg.eventHandler(event, window);
					}
					}, element);
			}
			

			if (event->is<sf::Event::Closed>()) { exitBox = true; }

			if (event->is<sf::Event::KeyPressed>()) {

				const auto* keyEvent = event->getIf<sf::Event::KeyPressed>();

				if (keyEvent->scancode == sf::Keyboard::Scan::Escape) {
					if (exitBox) { exitBox = false; }
					else { exitBox = true; }
				}
			}

			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && exitBox) {
				if (Ybutton.isMouseOver(window)) { window->close(); }
				else if (Nbutton.isMouseOver(window)) { exitBox = false; }
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
					if (GetOpenFileName(&openIMG)) { imgLoaded = true; }
				}

				if (dropletImg.isMouseOver(window)) {
					if (GetOpenFileName(&openDropletImg)) { dropletImgLoaded = true; }
				}

				if (newImg.isMouseOver(window)) {
					//Insert code that allows you to add a new image
				}

				if (fileButton.isMouseOver(window)) { fileDropdown.isVisible = true; }
				if (fileDropdown.isVisible) {
					if (fileDropdown.items[0].isMouseOver(window)) { createFile = true; }
					else if (fileDropdown.items[1].isMouseOver(window)) {
						GetOpenFileName(&openMIS);
						//add a line that updates the current workspace without disrupting previous workspaces
					}
					else if (fileDropdown.items[2].isMouseOver(window)) {
						//Insert code that allows you to save a file
					}
					else if (fileDropdown.items[3].isMouseOver(window)) {
						//Insert code that allows you to save a file as
					}
					else if (fileDropdown.items[4].isMouseOver(window)) { fileDropdown.isVisible = false; }
				}
				else { fileDropdown.isVisible = false; }

				if (!fileButton.isMouseOver(window) && fileDropdown.isVisible) { fileDropdown.isVisible = false; }
				
				for (auto& element : dropletRows) {
					std::visit([&window](auto&& arg) {
						using T = std::decay_t<decltype(arg)>;
						if constexpr (std::is_same_v<T, Droplet>) {
							if (arg.isMouseOveri(window)) { arg.isInitSelected = true; arg.isFinalSelected = false; }
							if (arg.isInitSelected) {
								for (int i = 0; i < arg.units.size(); i++) {
									if (arg.initUnitButtons[i].isMouseOver(window)) {
										int index = i;
										arg.setInitIndex(index);
										arg.changeInitText(arg.units[i]);
										
										arg.userOut.setString(arg.conversion(arg.input));
										arg.isInitSelected = false;
									}	
								}
							}
							if (arg.isMouseOverf(window)) { arg.isFinalSelected = true; arg.isInitSelected = false; }
							if (arg.isFinalSelected) {
								for (int i = 0; i < arg.units.size(); i++) {
									if (arg.finalUnitButtons[i].isMouseOver(window)) {
										int index = i;
										arg.setFinalIndex(index);
										arg.changeFinalText(arg.units[i]);
										arg.isFinalSelected = false;
										arg.userOut.setString(arg.conversion(arg.input));
										arg.isFinalSelected = false;
									}
								}
							}
						}
						}, element);
				}
			}

				if (imgDesc.isMouseOver(window)) { window->setMouseCursor(textCursor); }
				else { window->setMouseCursor(mainCursor); }

				if (dropletDesc.isMouseOver(window)) { window->setMouseCursor(textCursor); }
				else { window->setMouseCursor(mainCursor); }

				if (dropletTitle.isMouseOver(window)) { window->setMouseCursor(textCursor); }
				else { window->setMouseCursor(mainCursor); }

				for (auto& element : dropletRows) {
					std::visit([&window, &mainCursor, &textCursor](auto&& arg) {
						using T = std::decay_t<decltype(arg)>;
						if constexpr (std::is_same_v<T, textBox>) {
							if (arg.isMouseOver(window)) { window->setMouseCursor(textCursor); }
							else { window->setMouseCursor(mainCursor); }
						}
						else if constexpr (std::is_same_v<T, Droplet>) {
							if (arg.isMouseOverIO(window)) { window->setMouseCursor(textCursor); }
							else { window->setMouseCursor(mainCursor); }
						}
						}, element);
				}

		}

		projectName.update();
		fileLoc.update();
		imgDesc.update();
		desc.update();
		dropletDesc.update();
		dropletTitle.update();
		for (auto& element : dropletRows) {
			std::visit([](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, textBox>) { arg.update(); }
				else if constexpr (std::is_same_v<T, Droplet>) { arg.update(); }
				}, element);
		}
		
		


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
			dropletDesc.draw(window);
			dropletTitle.draw(window);
			dropletImg.draw(window);


			for (auto& element : dropletRows) {
				std::visit([&window](auto&& arg) {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, textBox>) { arg.draw(window); }
					else if constexpr (std::is_same_v<T, Droplet>) { arg.draw(window); }
					else if constexpr (std::is_same_v<T, sf::Sprite>) { window->draw(arg); }
					}, element);
			}

			if (imgLoaded) {

				sf::Texture imgInput(szFile, false, sf::IntRect({ 0, 0 }, { 650u, 510u }));
				sf::Sprite imgSprite(imgInput);

				imgSprite.setOrigin(imgSprite.getGlobalBounds().size / 2.0f);
				imgSprite.setPosition({ 1554, 377 });
				window->draw(imgSprite);
			}
			else { imgButton.draw(window); }

			if (dropletImgLoaded) {
				sf::Texture dropletInput(szFileDroplet, false, sf::IntRect({ 0, 0 }, { 223u, 267u }));
				sf::Sprite dropletSprite(dropletInput);

				dropletSprite.setOrigin(dropletSprite.getGlobalBounds().size / 2.0f);
				dropletSprite.setPosition({ 171, 272 });
				window->draw(dropletSprite);
			}
			else { dropletImg.draw(window); }
			
			fileButton.draw(window);
			fileDropdown.draw(window);
			window->draw(projectText);
			window->draw(projectNameBox);

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
/**
 *
 *
 */

#include "AnalyUtils.h"

#include "indicators/cursor_control.hpp"
#include "indicators/progress_bar.hpp"
#include "indicators/progress_spinner.hpp"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Regex.h"

#include "json/json.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace FGo
{
namespace Analy
{

void ProgressBar::safeDelete()
{
    if (m_pBar) {
        delete m_pBar;
        m_pBar = nullptr;
    }
}

ProgressBar::~ProgressBar()
{
    safeDelete();
}

ProgressBar::ProgressBar(const ProgressBar &_other)
{
    m_maxCount = _other.m_maxCount;
    m_curCount = _other.m_curCount;
    m_frontHint = _other.m_frontHint;
    this->safeDelete();
}

ProgressBar &ProgressBar::operator=(const ProgressBar &_other)
{
    m_maxCount = _other.m_maxCount;
    m_curCount = _other.m_curCount;
    m_frontHint = _other.m_frontHint;
    this->safeDelete();

    return *this;
}

void ProgressBar::
    start(uint64_t _maxCount, const String &_frontHint, bool notUsingBar /*=false*/)
{
    m_curCount = 0;
    m_maxCount = _maxCount;
    m_frontHint = _frontHint;

    indicators::show_console_cursor(false);

    safeDelete();

    m_notUsingBar = notUsingBar;
    if (!m_notUsingBar) {
        m_pBar = new indicators::BlockProgressBar();

        m_pBar->set_option(indicators::option::BarWidth{46});
        m_pBar->set_option(indicators::option::ForegroundColor{indicators::Color::cyan});
        m_pBar->set_option(indicators::option::FontStyles{
            Vector<indicators::FontStyle>{indicators::FontStyle::bold}
        });
        m_pBar->set_option(indicators::option::MaxProgress{_maxCount});

        std::cout << std::endl;
        std::cout << _frontHint << " (count = " << _maxCount << ")\n";

        m_pBar->set_option(
            indicators::option::PostfixText{toString(m_curCount) + "/" + toString(m_maxCount)}
        );
        m_pBar->tick();
    }
    else {
        std::cout << std::endl;
        std::cout << _frontHint << "\n";
    }
}

void ProgressBar::stop()
{
    if (!m_notUsingBar) {
        if (!m_pBar->is_completed()) m_pBar->mark_as_completed();
    }
    else {
        std::cout << termcolor::bold << termcolor::green << "Completed!\n" << termcolor::reset;
    }

    indicators::show_console_cursor(true);

    safeDelete();

    std::cout << std::endl;
}

void ProgressBar::show(const String &currentHint)
{
    if (!m_notUsingBar) {
        UniqueLock lock(m_mutex);

        m_pBar->set_option(indicators::option::PostfixText{
            toString(++m_curCount) + "/" + toString(m_maxCount) + " " + currentHint
        });
        m_pBar->tick();
    }
    else {
        std::cout << termcolor::bold << termcolor::cyan << currentHint << "\n"
                  << termcolor::reset;
    }
}

void OutputCapture::restore()
{
    if (m_streamBuffer) {
        std::cout.rdbuf(m_streamBuffer);
        m_streamBuffer = nullptr;
    }
}

OutputCapture::~OutputCapture()
{
    restore();
}

void OutputCapture::start()
{
    restore();
    m_sStream.clear();

    m_streamBuffer = std::cout.rdbuf();
    std::cout.rdbuf(m_sStream.rdbuf());
}

void OutputCapture::stop()
{
    restore();
}

String OutputCapture::getCapturedContent()
{
    return m_sStream.str();
}

StringVector splitString(const String &input, const String &delimiter)
{
    StringVector tokens;
    size_t start = 0, end = 0;

    while ((end = input.find(delimiter, start)) != String::npos) {
        tokens.push_back(input.substr(start, end - start));
        start = end + delimiter.length();
    }

    tokens.push_back(input.substr(start)); // To get the last token

    return tokens;
}

String trimString(const String &input, const String &space /*=""*/)
{
    size_t start = 0, end = input.size() - 1;
    if (space.empty()) {
        start = input.find_first_not_of(" \t\n\r"); // Find the first non-whitespace character
        end = input.find_last_not_of(" \t\n\r");    // Find the last non-whitespace character
    }
    else {
        start = input.find_first_not_of(space);
        end = input.find_last_not_of(space);
    }

    if (start == String::npos || end == String::npos) {
        // The string is empty or contains only whitespaces
        return "";
    }

    return input.substr(start, end - start + 1);
}

void replaceString(String &str, const String &origin, const String &replacement)
{
    size_t pos = str.find(origin);
    while (pos != String::npos) {
        str.replace(pos, origin.size(), replacement);
        pos = str.find(origin, pos + replacement.size());
    }
}

String getNodeIDString(uint32_t nodeID)
{
    std::stringstream ss;
    ss << "Node0x" << std::hex << nodeID;
    return ss.str();
}

void parseSVFLocationString(
    const String &sourceLoc, unsigned &line, unsigned &column, String &file
)
{
    line = 0;
    column = 0;
    file = "";

    String modiSrcLoc = sourceLoc;
    size_t pos = modiSrcLoc.find("\"basic block\"");
    if (pos != String::npos) {
        size_t pos2 = modiSrcLoc.find(",", pos);
        if (pos2 == String::npos) return;
        else modiSrcLoc = modiSrcLoc.substr(0, pos) + modiSrcLoc.substr(pos2 + 1);
    }

    Json::Value root;
    JSONCPP_STRING err;
    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(modiSrcLoc.c_str(), modiSrcLoc.c_str() + modiSrcLoc.size(), &root, &err))
        return;
    if (root.isMember("location")) root = root["location"];

    if (root.isMember("ln")) line = root["ln"].asUInt();
    if (root.isMember("cl")) column = root["cl"].asUInt();
    if (root.isMember("fl")) file = root["fl"].asString();
    if (root.isMember("file")) file = root["file"].asString();
}

void getLesserVector(
    Vector<int32_t> &result, const Vector<int32_t> &vec1, const Vector<int32_t> &vec2
)
{
    for (size_t i = 0; i < vec1.size() && i < vec2.size(); ++i) {
        if (vec1[i] >= 0 && vec2[i] >= 0) {
            result[i] = std::min(vec1[i], vec2[i]);
        }
        else if (vec1[i] < 0 && vec2[i] < 0) {
            result[i] = vec1[i];
        }
        else if (vec1[i] < 0) {
            result[i] = vec2[i];
        }
        else {
            result[i] = vec1[i];
        }
    }
}

void getLesserVector(
    Vector<int32_t> &modiVec, const Vector<int32_t> &oriVec, size_t vecSize,
    int32_t delta /*=0*/
)
{
    if (modiVec.size() < vecSize || oriVec.size() < vecSize)
        throw AnalyException("Invalid read of a vector");

    for (size_t i = 0; i < vecSize; ++i) {
        if (oriVec[i] >= 0 && (modiVec[i] < 0 || modiVec[i] > oriVec[i] + delta))
            modiVec[i] = oriVec[i] + delta;
    }
}

void getLesserVector(
    Json::Value &modiJsonValue, const Vector<int32_t> &oriVec, size_t vecSize,
    int32_t delta /*=0*/
)
{
    if (modiJsonValue.size() < vecSize || oriVec.size() < vecSize)
        throw AnalyException("Invalid read of a vector or a Json value");

    for (Json::Value::ArrayIndex i = 0; i < vecSize; ++i) {
        if (oriVec[i] >= 0 && (modiJsonValue[i] < 0 || modiJsonValue[i] > oriVec[i] + delta))
            modiJsonValue[i] = oriVec[i] + delta;
    }
}

void getNonNegativeVector(
    Vector<int32_t> &modiVec, const Vector<int32_t> &oriVec, size_t vecSize
)
{
    if (modiVec.size() < vecSize || oriVec.size() < vecSize)
        throw AnalyException("Invalid read of a vector");

    for (size_t i = 0; i < vecSize; ++i) {
        if (oriVec[i] >= 0 && (modiVec[i] < 0)) modiVec[i] = oriVec[i];
    }
}

void updateVectorWithDelta(Vector<int32_t> &modiVec, int32_t delta)
{
    for (auto &value : modiVec) value = value < 0 ? value : (value + delta);
}

bool pathExists(const String &filePath)
{
    return llvm::sys::fs::exists(filePath);
}

bool pathIsFile(const String &filePath)
{
    return llvm::sys::fs::is_regular_file(filePath);
}

bool pathIsDirectory(const String &filePath)
{
    return llvm::sys::fs::is_directory(filePath);
}

String joinPath(const String &basePath, const String &fileName)
{
    if (basePath.empty()) return fileName;

    llvm::SmallString<PATH_MAX> llvmBasePath(basePath);

    llvm::sys::path::append(llvmBasePath, fileName);

    return llvmBasePath.str().str();
}

int64_t getFileSize(const String &filePath)
{
    uint64_t fileSize = 0;
    auto ec = llvm::sys::fs::file_size(filePath, fileSize);
    if (ec) return -1;
    else return fileSize;
}

void getFileNameAndDirectory(const String &filePath, String &fileName, String &fileDir)
{
    llvm::SmallString<PATH_MAX> realPath;
    fileName = "";
    fileDir = "";
    if (llvm::sys::fs::real_path(filePath, realPath)) {
        return;
    }
    else {
        fileName = llvm::sys::path::filename(realPath).str();
        fileDir = llvm::sys::path::parent_path(realPath).str();
    }
}

void getMatchedFiles(const String &fileDir, const String &pattern, StringVector &matchedFiles)
{
    matchedFiles.clear();

    auto fileNamePattern = pattern;
    replaceString(fileNamePattern, ".", "\\.");
    replaceString(fileNamePattern, "*", ".*");

    llvm::Regex fileNameRegex(fileNamePattern);
    std::error_code ec;

    for (llvm::sys::fs::directory_iterator File(fileDir, ec), End; File != End && !ec;
         File.increment(ec))
    {
        llvm::StringRef fileName = llvm::sys::path::filename(File->path());

        // Check if the file name matches the specified pattern
        if (fileNameRegex.match(fileName)) matchedFiles.push_back(File->path());
    }
}

String getCurrentPath()
{
    llvm::SmallString<PATH_MAX> currentPath;
    if (llvm::sys::fs::current_path(currentPath)) return "";
    else return currentPath.str().str();
}

String getExeDirPath()
{
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        std::string executablePath(buffer);
        return llvm::sys::path::parent_path(executablePath).str();
    }
    else {
        return "";
    }
}
} // namespace Analy
} // namespace FGo
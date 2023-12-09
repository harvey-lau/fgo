/**
 *
 *
 */

#ifndef ANALYUTILS_H_
#define ANALYUTILS_H_

#include "indicators/block_progress_bar.hpp"

#include "json/json.h"
#include <array>
#include <iostream>
#include <list>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace FGo
{
namespace Analy
{
template <class _Tp>
struct Hash
{
    size_t operator()(const _Tp &t) const
    {
        std::hash<_Tp> h;
        return h(t);
    }
};

using String = std::string;

template <typename _Tp, typename _Allocator = std::allocator<_Tp>>
using Vector = std::vector<_Tp, _Allocator>;

template <typename _Tp, typename _Allocator = std::allocator<_Tp>>
using List = std::list<_Tp, _Allocator>;

template <typename _Tp, std::size_t _Nm>
using Array = std::array<_Tp, _Nm>;

template <typename _Tp, typename _Sequence = std::deque<_Tp>>
using Queue = std::queue<_Tp, _Sequence>;

using StringVector = std::vector<std::string>;

template <typename _Tp1, typename _Tp2>
using Pair = std::pair<_Tp1, _Tp2>;

template <
    typename _Key, typename _Value, typename _Hash = Hash<_Key>,
    typename _Equal = std::equal_to<_Key>,
    typename _Allocator = std::allocator<std::pair<const _Key, _Value>>>
using Map = std::unordered_map<_Key, _Value, _Hash, _Equal, _Allocator>;

template <
    typename _Key, typename _Hash = Hash<_Key>, typename _Equal = std::equal_to<_Key>,
    typename _Allocator = std::allocator<_Key>>
using Set = std::unordered_set<_Key, _Hash, _Equal, _Allocator>;

using Mutex = std::mutex;

using UniqueLock = std::unique_lock<Mutex>;

/// @brief Exception when analyzing ICFG
class AnalyException : public std::exception
{
private:
    String m_msg;

public:
    AnalyException(const char *message) : m_msg(message)
    {}
    AnalyException(const String &message) : m_msg(message)
    {}

    // Override the what() function to provide error message
    const char *what() const noexcept override
    {
        return m_msg.c_str();
    }
};

class UnexpectedException : public AnalyException
{
public:
    UnexpectedException(const char *message) : AnalyException(message)
    {}
    UnexpectedException(const String &message) : AnalyException(message)
    {}
};

class InvalidDataSetException : public AnalyException
{
public:
    InvalidDataSetException(const char *message) : AnalyException(message)
    {}
    InvalidDataSetException(const String &message) : AnalyException(message)
    {}
};

/// @brief A simple class for progress bar
class ProgressBar
{
private:
    uint64_t m_maxCount;
    uint64_t m_curCount;
    String m_frontHint;

    Mutex m_mutex;

    bool m_notUsingBar;

    indicators::BlockProgressBar *m_pBar;

    void safeDelete();

public:
    ProgressBar() :
        m_maxCount(0), m_curCount(0), m_frontHint(""), m_pBar(nullptr), m_notUsingBar(false)
    {}

    ProgressBar(uint64_t _maxCount, const String &_frontHint) :
        m_maxCount(_maxCount), m_curCount(0), m_frontHint(_frontHint), m_pBar(nullptr),
        m_notUsingBar(false)
    {}

    ProgressBar(const ProgressBar &_other);

    ProgressBar &operator=(const ProgressBar &_other);

    ~ProgressBar();

    /// @brief Start the progress bar.
    void start(uint64_t _maxCount, const String &_frontHint, bool notUsingBar = false);

    /// @brief Stop the progress bar.
    void stop();

    /// @brief Show the current progress.
    /// @param currentCount current count
    /// @param currentHint current hint
    void show(const String &currentHint);
};

class OutputCapture
{
private:
    std::streambuf *m_streamBuffer;
    std::stringstream m_sStream;

    void restore();

public:
    OutputCapture() : m_streamBuffer(nullptr)
    {}
    ~OutputCapture();

    void start();

    void stop();

    String getCapturedContent();
};

template <typename _Tp>
String toString(_Tp _value)
{
    return std::to_string(_value);
}

/// @brief Split a string with a separator
/// @param input original string
/// @param delimiter delimiter
/// @return A vector containing strings
StringVector splitString(const String &input, const String &delimiter);

/// @brief Strip the characters on the left end and the right end.
/// The default is whitespace.
/// @param input
/// @return
String trimString(const String &input, const String &space = "");

void replaceString(String &str, const String &origin, const String &replacement);

/// @brief Convert a node ID to a string
/// @param nodeID Node ID in SVF
/// @return A node ID string
String getNodeIDString(uint32_t nodeID);

/// @brief Parse a location string from SVF
/// @param sourceLoc
/// @param line
/// @param column
/// @param file
/// @exception `AnalyException`
void parseSVFLocationString(
    const String &sourceLoc, unsigned &line, unsigned &column, String &file
);

/// @brief Get the vector with
/// @param result
/// @param vec1
/// @param vec2
void getLesserVector(
    Vector<int32_t> &result, const Vector<int32_t> &vec1, const Vector<int32_t> &vec2
);

/// @brief Get the lesser non-negative vector
/// @param modiVec
/// @param oriVec
/// @param vecSize
/// @param delta
void getLesserVector(
    Vector<int32_t> &modiVec, const Vector<int32_t> &oriVec, size_t vecSize, int32_t delta = 0
);

/// @brief Get the lesser non-negative vector in Json
/// @param modiJsonValue
/// @param oriVec
/// @param vecSize
/// @param delta
void getLesserVector(
    Json::Value &modiJsonValue, const Vector<int32_t> &oriVec, size_t vecSize, int32_t delta = 0
);

/// @brief Get the non-negative vector without comparing two values
/// @param modiVec
/// @param oriVec
/// @param vecSize
void getNonNegativeVector(
    Vector<int32_t> &modiVec, const Vector<int32_t> &oriVec, size_t vecSize
);

/// @brief Update the non-negative vector with a delta value
/// @param modiVec
/// @param delta
void updateVectorWithDelta(Vector<int32_t> &modiVec, int32_t delta);

/// @brief Check whether the file path exists.
/// @param path
/// @return
bool pathExists(const String &path);

/// @brief Check whether the file path points to a regular file.
/// @param path
/// @return
bool pathIsFile(const String &path);

/// @brief Check whether the file path points to a directory.
/// @param path
/// @return
bool pathIsDirectory(const String &path);

/// @brief Get file size
/// @param filePath
/// @return the file size, otherwise -1
int64_t getFileSize(const String &filePath);

/// @brief Get the file name and the parent directory.
/// If errors occur, the `fileName` and `fileDir` will be made empty
/// @param filePath
/// @param fileName
/// @param fileDir
void getFileNameAndDirectory(const String &filePath, String &fileName, String &fileDir);

/// @brief Get the matched files with a regex pattern like "binary.0.0.*.bc"
/// @param fileDir
/// @param pattern
/// @param matchedFiles
void getMatchedFiles(const String &fileDir, const String &pattern, StringVector &matchedFiles);

/// @brief Get the directory where current executable locates
/// @return the parent directory
String getExeDirPath();

/// @brief Get the current working directory (CWD)
/// @return
String getCurrentPath();

/// @brief Join the base path and file name
/// @param basePath
/// @param fileName
/// @return The joined new path
String joinPath(const String &basePath, const String &fileName);
} // namespace Analy
} // namespace FGo

#endif
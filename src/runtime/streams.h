// streams.h -- Ardent 2.4 "Living Chronicles"
// Buffered I/O streams: ScrollStream and Scribe
#ifndef ARDENT_STREAMS_H
#define ARDENT_STREAMS_H

#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace ardent {
namespace streams {

// ─── Stream Mode ───────────────────────────────────────────────────────────

enum class StreamMode {
    Read,       // Open for reading
    Write,      // Open for writing (truncate)
    Append,     // Open for appending
    ReadWrite   // Open for both reading and writing
};

inline const char* streamModeToString(StreamMode m) {
    switch (m) {
        case StreamMode::Read: return "read";
        case StreamMode::Write: return "write";
        case StreamMode::Append: return "append";
        case StreamMode::ReadWrite: return "read-write";
        default: return "unknown";
    }
}

// ─── Stream State ──────────────────────────────────────────────────────────

enum class StreamState {
    Open,
    Closed,
    Error,
    EndOfFile
};

// ─── Scroll Stream (Base) ──────────────────────────────────────────────────

class ScrollStream {
public:
    virtual ~ScrollStream() = default;
    
    virtual bool isOpen() const = 0;
    virtual bool isEof() const = 0;
    virtual bool hasError() const = 0;
    virtual void close() = 0;
    
    virtual std::string path() const = 0;
    virtual StreamMode mode() const = 0;
    virtual StreamState state() const = 0;
    
    // Reading
    virtual std::optional<std::string> readLine() = 0;
    virtual std::optional<std::string> readAll() = 0;
    virtual std::optional<char> readChar() = 0;
    virtual std::optional<std::string> read(size_t n) = 0;
    
    // Writing
    virtual bool write(const std::string& data) = 0;
    virtual bool writeLine(const std::string& line) = 0;
    virtual bool flush() = 0;
    
    // Position
    virtual size_t position() const = 0;
    virtual bool seek(size_t pos) = 0;
    virtual size_t size() const = 0;
};

// ─── Scribe (File Stream Implementation) ───────────────────────────────────

class Scribe : public ScrollStream {
public:
    Scribe() = default;
    
    Scribe(const std::string& path, StreamMode mode)
        : path_(path), mode_(mode), state_(StreamState::Closed) {
        open(path, mode);
    }
    
    ~Scribe() override {
        close();
    }
    
    // Non-copyable, movable
    Scribe(const Scribe&) = delete;
    Scribe& operator=(const Scribe&) = delete;
    Scribe(Scribe&& other) noexcept { *this = std::move(other); }
    Scribe& operator=(Scribe&& other) noexcept {
        if (this != &other) {
            close();
            path_ = std::move(other.path_);
            mode_ = other.mode_;
            state_ = other.state_;
            file_ = std::move(other.file_);
            other.state_ = StreamState::Closed;
        }
        return *this;
    }
    
    bool open(const std::string& path, StreamMode mode) {
        close();
        path_ = path;
        mode_ = mode;
        
        std::ios::openmode flags = std::ios::binary;
        switch (mode) {
            case StreamMode::Read:
                flags |= std::ios::in;
                break;
            case StreamMode::Write:
                flags |= std::ios::out | std::ios::trunc;
                break;
            case StreamMode::Append:
                flags |= std::ios::out | std::ios::app;
                break;
            case StreamMode::ReadWrite:
                flags |= std::ios::in | std::ios::out;
                break;
        }
        
        file_.open(path, flags);
        
        if (file_.is_open()) {
            state_ = StreamState::Open;
            return true;
        } else {
            state_ = StreamState::Error;
            return false;
        }
    }
    
    bool isOpen() const override { return state_ == StreamState::Open && file_.is_open(); }
    bool isEof() const override { return state_ == StreamState::EndOfFile || file_.eof(); }
    bool hasError() const override { return state_ == StreamState::Error || file_.bad(); }
    
    void close() override {
        if (file_.is_open()) {
            file_.close();
        }
        state_ = StreamState::Closed;
    }
    
    std::string path() const override { return path_; }
    StreamMode mode() const override { return mode_; }
    StreamState state() const override { return state_; }
    
    // Reading
    std::optional<std::string> readLine() override {
        if (!isOpen() || mode_ == StreamMode::Write || mode_ == StreamMode::Append) {
            return std::nullopt;
        }
        
        std::string line;
        if (std::getline(file_, line)) {
            return line;
        }
        
        if (file_.eof()) {
            state_ = StreamState::EndOfFile;
        }
        return std::nullopt;
    }
    
    std::optional<std::string> readAll() override {
        if (!isOpen() || mode_ == StreamMode::Write || mode_ == StreamMode::Append) {
            return std::nullopt;
        }
        
        std::stringstream ss;
        ss << file_.rdbuf();
        state_ = StreamState::EndOfFile;
        return ss.str();
    }
    
    std::optional<char> readChar() override {
        if (!isOpen() || mode_ == StreamMode::Write || mode_ == StreamMode::Append) {
            return std::nullopt;
        }
        
        char c;
        if (file_.get(c)) {
            return c;
        }
        
        if (file_.eof()) {
            state_ = StreamState::EndOfFile;
        }
        return std::nullopt;
    }
    
    std::optional<std::string> read(size_t n) override {
        if (!isOpen() || mode_ == StreamMode::Write || mode_ == StreamMode::Append) {
            return std::nullopt;
        }
        
        std::string buffer(n, '\0');
        file_.read(&buffer[0], n);
        size_t bytesRead = file_.gcount();
        
        if (bytesRead == 0 && file_.eof()) {
            state_ = StreamState::EndOfFile;
            return std::nullopt;
        }
        
        buffer.resize(bytesRead);
        return buffer;
    }
    
    // Writing
    bool write(const std::string& data) override {
        if (!isOpen() || mode_ == StreamMode::Read) {
            return false;
        }
        
        file_ << data;
        return !file_.bad();
    }
    
    bool writeLine(const std::string& line) override {
        if (!isOpen() || mode_ == StreamMode::Read) {
            return false;
        }
        
        file_ << line << '\n';
        return !file_.bad();
    }
    
    bool flush() override {
        if (!isOpen()) return false;
        file_.flush();
        return !file_.bad();
    }
    
    // Position
    size_t position() const override {
        if (!file_.is_open()) return 0;
        return static_cast<size_t>(const_cast<std::fstream&>(file_).tellg());
    }
    
    bool seek(size_t pos) override {
        if (!isOpen()) return false;
        file_.seekg(pos);
        file_.seekp(pos);
        return !file_.fail();
    }
    
    size_t size() const override {
        if (!file_.is_open()) return 0;
        auto& f = const_cast<std::fstream&>(file_);
        auto curr = f.tellg();
        f.seekg(0, std::ios::end);
        auto sz = f.tellg();
        f.seekg(curr);
        return static_cast<size_t>(sz);
    }
    
private:
    std::string path_;
    StreamMode mode_ = StreamMode::Read;
    StreamState state_ = StreamState::Closed;
    std::fstream file_;
};

// ─── String Stream (In-Memory) ─────────────────────────────────────────────

class StringScribe : public ScrollStream {
public:
    StringScribe() : state_(StreamState::Open) {}
    explicit StringScribe(const std::string& initial) : buffer_(initial), state_(StreamState::Open) {}
    
    bool isOpen() const override { return state_ == StreamState::Open; }
    bool isEof() const override { return pos_ >= buffer_.size(); }
    bool hasError() const override { return state_ == StreamState::Error; }
    void close() override { state_ = StreamState::Closed; }
    
    std::string path() const override { return "<string>"; }
    StreamMode mode() const override { return StreamMode::ReadWrite; }
    StreamState state() const override { return state_; }
    
    std::optional<std::string> readLine() override {
        if (!isOpen() || isEof()) return std::nullopt;
        
        size_t start = pos_;
        while (pos_ < buffer_.size() && buffer_[pos_] != '\n') {
            ++pos_;
        }
        
        std::string line = buffer_.substr(start, pos_ - start);
        if (pos_ < buffer_.size()) ++pos_; // skip newline
        
        return line;
    }
    
    std::optional<std::string> readAll() override {
        if (!isOpen()) return std::nullopt;
        std::string result = buffer_.substr(pos_);
        pos_ = buffer_.size();
        return result;
    }
    
    std::optional<char> readChar() override {
        if (!isOpen() || isEof()) return std::nullopt;
        return buffer_[pos_++];
    }
    
    std::optional<std::string> read(size_t n) override {
        if (!isOpen() || isEof()) return std::nullopt;
        size_t available = buffer_.size() - pos_;
        size_t toRead = std::min(n, available);
        std::string result = buffer_.substr(pos_, toRead);
        pos_ += toRead;
        return result;
    }
    
    bool write(const std::string& data) override {
        if (!isOpen()) return false;
        buffer_ += data;
        return true;
    }
    
    bool writeLine(const std::string& line) override {
        if (!isOpen()) return false;
        buffer_ += line + '\n';
        return true;
    }
    
    bool flush() override { return isOpen(); }
    
    size_t position() const override { return pos_; }
    bool seek(size_t pos) override {
        if (!isOpen()) return false;
        pos_ = std::min(pos, buffer_.size());
        return true;
    }
    size_t size() const override { return buffer_.size(); }
    
    // Direct access to buffer
    const std::string& str() const { return buffer_; }
    void setStr(const std::string& s) { buffer_ = s; pos_ = 0; }
    
private:
    std::string buffer_;
    size_t pos_ = 0;
    StreamState state_ = StreamState::Open;
};

// ─── Line Iterator ─────────────────────────────────────────────────────────

class LineIterator {
public:
    explicit LineIterator(ScrollStream* stream) : stream_(stream) {
        if (stream_ && stream_->isOpen() && !stream_->isEof()) {
            advance();
        }
    }
    
    bool hasNext() const { return hasLine_; }
    
    const std::string& current() const { return currentLine_; }
    
    void advance() {
        if (!stream_ || !stream_->isOpen()) {
            hasLine_ = false;
            return;
        }
        
        auto line = stream_->readLine();
        if (line) {
            currentLine_ = *line;
            hasLine_ = true;
        } else {
            hasLine_ = false;
        }
    }
    
private:
    ScrollStream* stream_;
    std::string currentLine_;
    bool hasLine_ = false;
};

// ─── Stream Manager ────────────────────────────────────────────────────────

class StreamManager {
public:
    using StreamId = uint64_t;
    
    StreamId openFile(const std::string& path, StreamMode mode) {
        auto scribe = std::make_unique<Scribe>(path, mode);
        if (!scribe->isOpen()) {
            return 0; // 0 = invalid
        }
        StreamId id = nextId_++;
        streams_[id] = std::move(scribe);
        return id;
    }
    
    StreamId createString(const std::string& initial = "") {
        StreamId id = nextId_++;
        streams_[id] = std::make_unique<StringScribe>(initial);
        return id;
    }
    
    ScrollStream* get(StreamId id) {
        auto it = streams_.find(id);
        if (it == streams_.end()) return nullptr;
        return it->second.get();
    }
    
    bool close(StreamId id) {
        auto it = streams_.find(id);
        if (it == streams_.end()) return false;
        it->second->close();
        streams_.erase(it);
        return true;
    }
    
    void closeAll() {
        for (auto& [id, stream] : streams_) {
            stream->close();
        }
        streams_.clear();
    }
    
    size_t activeCount() const { return streams_.size(); }
    
private:
    StreamId nextId_ = 1;
    std::unordered_map<StreamId, std::unique_ptr<ScrollStream>> streams_;
};

// ─── Global Stream Manager ─────────────────────────────────────────────────

inline StreamManager& globalStreamManager() {
    static StreamManager manager;
    return manager;
}

} // namespace streams
} // namespace ardent

#endif // ARDENT_STREAMS_H

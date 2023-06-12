#pragma once

#include <forward_list>
#include <memory>
#include <string_view>
#include <string>
#include <vector>

#include "fredbuf-rbtree.h"
#include "types.h"

#ifndef NDEBUG
#define TEXTBUF_DEBUG
#endif // NDEBUG

// This is a C++ implementation of the textbuf data structure described in
// https://code.visualstudio.com/blogs/2018/03/23/text-buffer-reimplementation. The differences are
// that this version is based on immutable data structures to achieve fast undo/redo.
namespace PieceTree
{
    struct UndoRedoEntry
    {
        RedBlackTree root;
        CharOffset op_offset;
    };

    // We need the ability to 'release' old entries in this stack.
    using UndoStack = std::forward_list<UndoRedoEntry>;
    using RedoStack = std::forward_list<UndoRedoEntry>;

    enum class LineStart : size_t { };

    using LineStarts = std::vector<LineStart>;

    struct NodePosition
    {
        // Piece Index
        const PieceTree::NodeData* node = nullptr;
        // Remainder in current piece.
        Length remainder = { };
        // Node start offset in document.
        CharOffset start_offset = { };
        // The line (relative to the document) where this node starts.
        Line line = { };
    };

    struct CharBuffer
    {
        std::string buffer;
        LineStarts line_starts;
    };

    using BufferReference = std::shared_ptr<const CharBuffer>;

    using Buffers = std::vector<BufferReference>;

    struct LineRange
    {
        CharOffset first;
        CharOffset last; // Does not include LF.
    };

    struct UndoRedoResult
    {
        bool success;
        CharOffset op_offset;
    };

    // Should only be used to uniquely identify a given tree.
    using RootIdentity = const void*;

    struct Tree
    {
        explicit Tree();
        explicit Tree(Buffers&& buffers);

        // Interface.
        // Initialization after populating initial immutable buffers from ctor.
        void build_tree();

        // Manipulation.
        void insert(CharOffset offset, std::string_view txt);
        void remove(CharOffset offset, Length count);
        UndoRedoResult try_undo(CharOffset op_offset);
        UndoRedoResult try_redo(CharOffset op_offset);

        // Queries.
        void get_line_content(std::string* buf, Line line) const;
        void get_line_content_crlf(std::string* buf, Line line) const;
        char at(CharOffset offset) const;
        Line line_at(CharOffset offset) const;
        LineRange get_line_range(Line line) const;
        LineRange get_line_range_crlf(Line line) const;
        LineRange get_line_range_with_newline(Line line) const;
        RootIdentity root_identity() const;

        Length length() const
        {
            return meta.total_content_length;
        }

        bool is_empty() const
        {
            return meta.total_content_length == Length{};
        }

        LFCount line_feed_count() const
        {
            return meta.lf_count;
        }

        Length line_count() const
        {
            return Length{ rep(line_feed_count()) + 1 };
        }
    private:
        friend class TreeWalker;
#ifdef TEXTBUF_DEBUG
        friend void print_piece(const Piece& piece, const Tree* tree, int level);
        friend void print_tree(const Tree& tree);
#endif // TEXTBUF_DEBUG

        using Accumulator = Length(Tree::*)(const Piece&, Line) const;

        template <Accumulator accumulate>
        void line_start(CharOffset* offset, const PieceTree::RedBlackTree& node, Line line) const;
        void line_end_crlf(CharOffset* offset, const PieceTree::RedBlackTree& node, Line line) const;
        Length accumulate_value(const Piece& piece, Line index) const;
        Length accumulate_value_no_lf(const Piece& piece, Line index) const;
        CharOffset buffer_offset(BufferIndex index, const BufferCursor& cursor) const;
        void populate_from_node(std::string* buf, const PieceTree::RedBlackTree& node) const;
        void populate_from_node(std::string* buf, const PieceTree::RedBlackTree& node, Line line_index) const;
        void assemble_line(std::string* buf, const PieceTree::RedBlackTree& node, Line line) const;
        LFCount line_feed_count(BufferIndex index, const BufferCursor& start, const BufferCursor& end);
        Piece build_piece(std::string_view txt);
        NodePosition node_at(CharOffset off) const;
        BufferCursor buffer_position(const Piece& piece, Length remainder) const;
        Piece trim_piece_right(const Piece& piece, const BufferCursor& pos);
        Piece trim_piece_left(const Piece& piece, const BufferCursor& pos);

        struct ShrinkResult
        {
            Piece left;
            Piece right;
        };

        ShrinkResult shrink_piece(const Piece& piece, const BufferCursor& first, const BufferCursor& last);
        void combine_pieces(NodePosition existing, Piece new_piece);
        void remove_node_range(NodePosition first, Length length);
        const CharBuffer* buffer_at(BufferIndex index) const;
        void compute_buffer_meta();
        void append_undo(const RedBlackTree& old_root, CharOffset op_offset);

        struct BufferMeta
        {
            LFCount lf_count = { };
            Length total_content_length = { };
        };

        Buffers buffers;
        CharBuffer mod_buffer;
        PieceTree::RedBlackTree root;
        LineStarts scratch_starts;
        BufferCursor last_insert;
        // Note: This is absolute position.  Initialize to nonsense value.
        CharOffset end_last_insert = CharOffset::Sentinel;
        BufferMeta meta;
        UndoStack undo_stack;
        RedoStack redo_stack;
    };

    struct TreeBuilder
    {
        Buffers buffers;
        LineStarts scratch_starts;

        void accept(std::string_view txt);

        Tree create()
        {
            return Tree{ std::move(buffers) };
        }
    };

    class TreeWalker
    {
    public:
        TreeWalker(const Tree* tree, CharOffset offset = CharOffset{ });
        TreeWalker(const TreeWalker&) = delete;

        char current();
        char next();
        void seek(CharOffset offset);
        bool exhausted() const;
        Length remaining() const;
        CharOffset offset() const
        {
            return total_offset;
        }

        // For Iterator-like behavior.
        TreeWalker& operator++()
        {
            return *this;
        }

        char operator*()
        {
            return next();
        }
    private:
        void populate_ptrs();
        void fast_forward_to(CharOffset offset);

        enum class Direction { Left, Center, Right };

        struct StackEntry
        {
            PieceTree::RedBlackTree node;
            Direction dir = Direction::Left;
        };

        const Tree* tree;
        std::vector<StackEntry> stack;
        CharOffset total_offset = CharOffset{ 0 };
        const char* first_ptr = nullptr;
        const char* last_ptr = nullptr;
    };

    struct WalkSentinel { };

    inline TreeWalker begin(const Tree& tree)
    {
        return TreeWalker{ &tree };
    }

    constexpr WalkSentinel end(const Tree&)
    {
        return WalkSentinel{ };
    }

    inline bool operator==(const TreeWalker& walker, WalkSentinel)
    {
        return walker.exhausted();
    }
} // namespace PieceTree
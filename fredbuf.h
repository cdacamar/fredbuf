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

    struct BufferCollection
    {
        const CharBuffer* buffer_at(BufferIndex index) const;
        CharOffset buffer_offset(BufferIndex index, const BufferCursor& cursor) const;

        Buffers orig_buffers;
        CharBuffer mod_buffer;
    };

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

    // Owning snapshot owns its own buffer data (performs a lightweight copy) so
    // that even if the original tree is destroyed, the owning snapshot can still
    // reference the underlying text.
    class OwningSnapshot;

    // Reference snapshot owns no data and is only valid for as long as the original
    // tree buffers are valid.
    class ReferenceSnapshot;

    // When mutating the tree nodes are saved by default into the undo stack.  This
    // allows callers to suppress this behavior.
    enum class SuppressHistory : bool { No, Yes };

    struct BufferMeta
    {
        LFCount lf_count = { };
        Length total_content_length = { };
    };

    // Indicates whether or not line was missing a CR (e.g. only a '\n' was at the end).
    enum class IncompleteCRLF : bool { No, Yes };

    class Tree
    {
    public:
        explicit Tree();
        explicit Tree(Buffers&& buffers);

        // Interface.
        // Initialization after populating initial immutable buffers from ctor.
        void build_tree();

        // Manipulation.
        void insert(CharOffset offset, std::string_view txt, SuppressHistory suppress_history = SuppressHistory::No);
        void remove(CharOffset offset, Length count, SuppressHistory suppress_history = SuppressHistory::No);
        UndoRedoResult try_undo(CharOffset op_offset);
        UndoRedoResult try_redo(CharOffset op_offset);

        // Direct history manipulation.
        // This will commit the current node to the history.  The offset provided will be the undo point later.
        void commit_head(CharOffset offset);
        RedBlackTree head() const;
        // Snaps the tree back to the specified root.  This needs to be called with a root that is derived from
        // the set of buffers based on its creation.
        void snap_to(const RedBlackTree& new_root);

        // Queries.
        void get_line_content(std::string* buf, Line line) const;
        [[nodiscard]] IncompleteCRLF get_line_content_crlf(std::string* buf, Line line) const;
        char at(CharOffset offset) const;
        Line line_at(CharOffset offset) const;
        LineRange get_line_range(Line line) const;
        LineRange get_line_range_crlf(Line line) const;
        LineRange get_line_range_with_newline(Line line) const;

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

        OwningSnapshot owning_snap() const;
        ReferenceSnapshot ref_snap() const;
    private:
        friend class TreeWalker;
        friend class OwningSnapshot;
        friend class ReferenceSnapshot;
#ifdef TEXTBUF_DEBUG
        friend void print_piece(const Piece& piece, const Tree* tree, int level);
        friend void print_tree(const Tree& tree);
#endif // TEXTBUF_DEBUG
        void internal_insert(CharOffset offset, std::string_view txt);
        void internal_remove(CharOffset offset, Length count);

        using Accumulator = Length(*)(const BufferCollection*, const Piece&, Line);

        template <Accumulator accumulate>
        static void line_start(CharOffset* offset, const BufferCollection* buffers, const RedBlackTree& node, Line line);
        static void line_end_crlf(CharOffset* offset, const BufferCollection* buffers, const RedBlackTree& root, const RedBlackTree& node, Line line);
        static Length accumulate_value(const BufferCollection* buffers, const Piece& piece, Line index);
        static Length accumulate_value_no_lf(const BufferCollection* buffers, const Piece& piece, Line index);
        static void populate_from_node(std::string* buf, const BufferCollection* buffers, const RedBlackTree& node);
        static void populate_from_node(std::string* buf, const BufferCollection* buffers, const RedBlackTree& node, Line line_index);
        static LFCount line_feed_count(const BufferCollection* buffers, BufferIndex index, const BufferCursor& start, const BufferCursor& end);
        static NodePosition node_at(const BufferCollection* buffers, RedBlackTree node, CharOffset off);
        static BufferCursor buffer_position(const BufferCollection* buffers, const Piece& piece, Length remainder);
        static char char_at(const BufferCollection* buffers, const RedBlackTree& node, CharOffset offset);
        static Piece trim_piece_right(const BufferCollection* buffers, const Piece& piece, const BufferCursor& pos);
        static Piece trim_piece_left(const BufferCollection* buffers, const Piece& piece, const BufferCursor& pos);

        struct ShrinkResult
        {
            Piece left;
            Piece right;
        };

        static ShrinkResult shrink_piece(const BufferCollection* buffers, const Piece& piece, const BufferCursor& first, const BufferCursor& last);

        // Direct mutations.
        void assemble_line(std::string* buf, const RedBlackTree& node, Line line) const;
        Piece build_piece(std::string_view txt);
        void combine_pieces(NodePosition existing_piece, Piece new_piece);
        void remove_node_range(NodePosition first, Length length);
        void compute_buffer_meta();
        void append_undo(const RedBlackTree& old_root, CharOffset op_offset);

        BufferCollection buffers;
        //Buffers buffers;
        //CharBuffer mod_buffer;
        PieceTree::RedBlackTree root;
        LineStarts scratch_starts;
        BufferCursor last_insert;
        // Note: This is absolute position.  Initialize to nonsense value.
        CharOffset end_last_insert = CharOffset::Sentinel;
        BufferMeta meta;
        UndoStack undo_stack;
        RedoStack redo_stack;
    };

    class OwningSnapshot
    {
    public:
        explicit OwningSnapshot(const Tree* tree);
        explicit OwningSnapshot(const Tree* tree, const RedBlackTree& dt);

        // Queries.
        void get_line_content(std::string* buf, Line line) const;
        [[nodiscard]] IncompleteCRLF get_line_content_crlf(std::string* buf, Line line) const;
        Line line_at(CharOffset offset) const;
        LineRange get_line_range(Line line) const;
        LineRange get_line_range_crlf(Line line) const;
        LineRange get_line_range_with_newline(Line line) const;
        bool is_empty() const
        {
            return meta.total_content_length == Length{};
        }

        Length line_count() const
        {
            return Length{ rep(meta.lf_count) + 1 };
        }
    private:
        friend class TreeWalker;

        RedBlackTree root;
        BufferMeta meta;
        // This should be fairly lightweight.  The original buffers
        // will retain the majority of the memory consumption.
        BufferCollection buffers;
    };

    class ReferenceSnapshot
    {
    public:
        explicit ReferenceSnapshot(const Tree* tree);
        explicit ReferenceSnapshot(const Tree* tree, const RedBlackTree& dt);

        // Queries.
        void get_line_content(std::string* buf, Line line) const;
        [[nodiscard]] IncompleteCRLF get_line_content_crlf(std::string* buf, Line line) const;
        Line line_at(CharOffset offset) const;
        LineRange get_line_range(Line line) const;
        LineRange get_line_range_crlf(Line line) const;
        LineRange get_line_range_with_newline(Line line) const;
        bool is_empty() const
        {
            return meta.total_content_length == Length{};
        }

        Length line_count() const
        {
            return Length{ rep(meta.lf_count) + 1 };
        }
    private:
        friend class TreeWalker;

        RedBlackTree root;
        BufferMeta meta;
        // A reference to the underlying tree buffers.
        const BufferCollection* buffers;
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
        TreeWalker(const OwningSnapshot* snap, CharOffset offset = CharOffset{ });
        TreeWalker(const ReferenceSnapshot* snap, CharOffset offset = CharOffset{ });
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

        const BufferCollection* buffers;
        RedBlackTree root;
        BufferMeta meta;
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

    enum class EmptySelection : bool { No, Yes };

    struct SelectionMeta
    {
        OwningSnapshot snap;
        Offset first;
        Offset last;
        EmptySelection empty;
    };
} // namespace PieceTree
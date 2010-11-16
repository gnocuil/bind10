// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef _RBTREE_H
#define _RBTREE_H 1

#include <dns/name.h>
#include <dns/rrset.h>
#include <dns/rrsetlist.h>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

using namespace isc::dns;
namespace isc {
namespace datasrc {

namespace {
/// helper function to remove the base domain from super domain
/// the precondition of this function is the super_name contains the
/// sub_name
Name
operator-(const Name& super_name, const Name& sub_name) {
    return (super_name.split(0, super_name.getLabelCount() -
                             sub_name.getLabelCount()));
}
}

/// \brief Define rbtree color
enum RBTreeColor {BLACK = 1, RED};
template <typename T>
class RBTree;

/// \brief \c RBNode class represents one domain name in the domain space

/// It has two roles, the first one is as one node in the \c RBTree,
/// the second one is to store the data related to DNS. As for the first role,
/// it has left, right, parent and color members
/// which is used to keep the balance of the \c RBTree. As for the second role,
//  it stores the RRsets that belong to the domain name and a rbtree which
/// includes all the subdomains of this node.
/// The name stored in the node is relative related to its parent node.
/// One special kind of node is non-terminal node
/// which has subdomains with RRset but itself doesn't have any RRsets.
///
/// \b Note: \c RBNode should be created or destroyed only by \c RBTree so
/// constructor and destructor function aren't exposed.
template <typename T>
class RBNode : public boost::noncopyable {
public:
    /// only /c RBTree can create and destroy \c RBNode
    friend class RBTree<T>;
    /// \name Test functions
    //@{
    /// \brief return the name of current node, it's relative to its parents
    //
    /// \todo Is it meaningful to return the absolute of the node?
    const Name& getName() const {return (name_); }

    // \breif return the data 
    T& getData() { return (data_);}
    // \brief return next node whose name is bigger than current node
    RBNode<T>* successor();
    //@}
    
    /// \name Modify functions
    // \brief return the data stored in the node
    void setData(T& data) { data_ = data;}

private:
    /// \name Constructors and destructor
    //@{
    /// \param nullnode The null point for \c RBNode isnot use \c NULL, but
    /// use one specified
    /// default node singled as null node, this is intended to keep the code
    /// more unify

    RBNode(const Name &name, RBNode<T> *nullnode = NULL);

    RBNode(const Name& name, T& data,
            RBNode<T>* nullnode = NULL);

    /// the class isn't left to be inherited
    ~RBNode();
    //@}

    /// \brief copy the DNS related data to another node except the sub domain
    /// tree
    void cloneDNSData(RBNode<T>& node);

    /// \brief when copy the DNS data from one node to another, except the
    /// RRsets, name etc,
    /// also needs to maintain the down and up relationship, which includes
    /// set the down point of current node and up point of sub domain tree
    void setDownTree(RBTree<T>* down);

    /// data to maintain the rbtree balance
    RBNode<T>*  parent_;
    RBNode<T>*  left_;
    RBNode<T>*  right_;
    RBTreeColor color_;

    /// data to carry dns info
    Name        name_;
    T           data_;
    RBTree<T>*  down_;
    bool        is_shadow_; ///the node willn't return to end user, if the node is shadow
};


template <typename T>
RBNode<T>::RBNode(const Name& name, T &data, RBNode* nullnode) :
    parent_(nullnode),
    left_(nullnode),
    right_(nullnode),
    color_(RED),
    name_(name),
    data_(data),
    down_(NULL) {
}

template <typename T>
RBNode<T>::RBNode(const Name& name, RBNode* nullnode) :
    parent_(nullnode),
    left_(nullnode),
    right_(nullnode),
    color_(RED),
    name_(name),
    down_(NULL) {
}

template <typename T>
RBNode<T>::~RBNode() {
    delete down_;
}

template <typename T>
RBNode<T>*
RBNode<T>::successor() {
    RBNode<T>* current = this;

    // if it has right node, the successor is the left-most node
    if (right_ != right_->right_) {
        current = right_;
        while (current->left_ != current->left_->left_) {
            current = current->left_;
        }
        return (current);
    }

    // otherwise return the parent without left child or
    // current node is not its right child
    RBNode<T>* s = current->parent_;
    while (s != s->left_ && current == s->right_) {
        current = s;
        s = s->parent_;
    }
    return (s);
}

template <typename T>
void
RBNode<T>::cloneDNSData(RBNode<T>& node) {
    node.name_ = name_;
    node.data_ = data_;
}

template <typename T>
void
RBNode<T>::setDownTree(RBTree<T>* down) {
    down_ = down;
    if (down) {
        down->up_ = this;
    }
}
/// \brief \c RBTree class represents all the domains with the same suffix,
/// so it can be used to store the domains in one zone.
///
/// \c RBTree is a generic red black tree, and contains all the nodes with
/// the same suffix, since each
/// name may have sub domain names so \c RBTree is a recursive data structure
/// or tree in tree.
/// So for one zone, several RBTrees may be involved. But from outside, the sub
/// tree is opaque for end users.
template <typename T>
class RBTree : public boost::noncopyable {
    friend class RBNode<T>;
public:
    /// \brief The return value for the \c find() method
    /// - EXACTMATCH: return the node in the tree exactly same with the target
    /// - FINDREFERRAL: return the node which is an ancestor of the target
    ///   containing NS record
    /// - NOTFOUND: other conditions except EXACTMATCH & FINDREFERRAL
    enum FindResult{EXACTMATCH, PARTIALMATCH, NOTFOUND};

    /// \name Constructor and Destructor
    //@{
    RBTree();

    /// \b Note: RBTree is not intended to be inherited so the destructor
    /// is not virtual
    ~RBTree();
    //@}

    /// \name Inquery methods
    //@{
    /// \brief Find the node with the name
    /// \param name Target to be found
    /// \param node Point to the node when the return vaule is \c not
    /// NOTFOUND, if the return value is NOTFOUND, the value of node is
    /// \c unknown
    FindResult find(const Name& name, RBNode<T>** node) const;

    /// \brief Get the total node count in the tree
    int getNodeCount() const;
    //@}

    /// \name Debug function
    //@{
    /// \brief print the nodes in the trees
    /// \todo is it better to return one string instead of print to the stdout?
    void printTree(int depth = 0) const;
    //@}

    /// \name Modify function
    //@{
    /// \brief Insert the domain name into the tree
    /// \param name The name to be inserted into the tree
    /// \param inserted_node If no node with the name in the tree,
    /// new \c RBNode will be created, otherwise nothing will be done.
    /// Anyway the pointer point to the node with the name will be assigned to
    /// inserted_node
    /// \return return 0 means no node exists in the tree with the name before
    /// insert; return 1 means already has the node with the given name
    //
    /// To add an RRset into one node when it's not known whether the node
    /// already exists, it is better to call \c insert and then call the
    /// RBNode interface instead of calling \c find().
    int insert(const Name& name, RBNode<T>** inserted_node);

    /// \brief Erase the node with the domain name
    /// \return If no node with the name, return 1; otherwise return 0
    int erase(const Name& name);
    //@}

private:
    /// \name RBTree balance functions
    //@{
    void deleteRebalance(RBNode<T>* node);
    void insertRebalance(RBNode<T>* node);
    RBNode<T>* rightRotate(RBNode<T>* p);
    RBNode<T>* leftRotate(RBNode<T>* p);
    //@}

    /// \name Helper functions
    //@{
    /// Each public function has related recursive helper function
    void eraseNode(RBNode<T>* node);
    FindResult findHelper(const Name& name, RBTree<T>** tree,
                          RBNode<T>** node) const;
    int getNodeCountHelper(const RBNode<T>* node) const;
    void printTreeHelper(RBNode<T>* node, int depth) const;
    //@}

    RBNode<T>*  root_;
    RBNode<T>*  NULLNODE;
    RBNode<T>*  up_;
    /// the node count of current tree except the sub domain trees
    unsigned int node_count_;
};

/*
 with the following names:
     a       x.d.e.f     o.w.y.d.e.f
     b       z.d.e.f     p.w.y.d.e.f
     c       g.h         q.w.y.d.e.f
     the tree will looks like:
                                  b
                                /   \
                               a    d.e.f
                                      /|\
                                     c | g.h
                                       |
                                      w.y
                                      /|\
                                     x | z
                                       |
                                       p
                                      / \
                                     o   q
*/
template <typename T>
RBTree<T>::RBTree() {
    NULLNODE = new RBNode<T>(Name(" "));
    NULLNODE->parent_  = NULLNODE->left_ = NULLNODE->right_ = NULLNODE;
    NULLNODE->color_ = BLACK;
    root_ = NULLNODE;
    node_count_ = 0;
    up_ = NULL;
}

template <typename T>
RBTree<T>::~RBTree() {
    assert(root_ != NULL);

    delete NULLNODE;
    if (root_ == NULLNODE) {
        return;
    }

    RBNode<T>* node = root_;
    while (root_->left_ != NULLNODE || root_->right_ != NULLNODE) {
        while (node->left_ != NULLNODE || node->right_ != NULLNODE) {
            node = (node->left_ != NULLNODE) ? node->left_ : node->right_;
        }

        RBNode<T>* parent = node->parent_;
        if (parent->left_ == node) {
            parent->left_ = NULLNODE;
        } else {
            parent->right_ = NULLNODE;
        }
        delete node;
        node = parent;
    }

    delete root_;
    root_ = NULL;
}

template <typename T>
typename RBTree<T>::FindResult
RBTree<T>::find(const Name& name, RBNode<T>** node) const {
    RBTree<T>* tree;
    return (findHelper(name, &tree, node));
}

template <typename T>
typename RBTree<T>::FindResult
RBTree<T>::findHelper(const Name& name, RBTree<T>** tree, RBNode<T>** ret) const {
    RBNode<T>* node = root_;
    while (node != NULLNODE) {
        NameComparisonResult compare_result = name.compare(node->name_);
        NameComparisonResult::NameRelation relation =
            compare_result.getRelation();
        if (relation == NameComparisonResult::EQUAL) {
            *tree = (RBTree*)this;
            *ret = node;
            return (RBTree<T>::EXACTMATCH);
        } else {
            int common_label_count = compare_result.getCommonLabels();
            // common label count equal one means, there is no common between
            // two names
            if (common_label_count == 1) {
                node = (compare_result.getOrder() < 0) ?
                    node->left_ : node->right_;
            } else if (NameComparisonResult::SUBDOMAIN == relation) {
                if (node->is_shadow_) {
                    assert(node->down_);
                    return (node->down_->findHelper(name - node->name_, tree,
                                    ret));
                } else {
                    RBTree<T>::FindResult result = RBTree<T>::NOTFOUND;
                    if (node->down_) {
                        result = node->down_->findHelper(name - node->name_, tree,
                                ret);
                    }
                    // if not found in sub domain tree, so current node is the longest match
                    // otherwise return the result in sub domin tree
                    if (RBTree<T>::NOTFOUND == result) {
                        *tree = (RBTree *)this;
                        *ret = node;
                        return RBTree<T>::PARTIALMATCH;
                    } else {
                        return result;
                    }
                }
            } else {
                return (RBTree<T>::NOTFOUND);
            }
        }
    }

    return (RBTree<T>::NOTFOUND);
}

template <typename T>
int
RBTree<T>::getNodeCount() const {
    return (getNodeCountHelper(root_));
}

template <typename T>
int
RBTree<T>::getNodeCountHelper(const RBNode<T> *node) const {
    if (NULLNODE == node) {
        return (0);
    }

    int sub_tree_node_count = node->down_ ? node->down_->getNodeCount() : 0;
    return (1 + sub_tree_node_count + getNodeCountHelper(node->left_) +
            getNodeCountHelper(node->right_));
}

template <typename T>
int
RBTree<T>::insert(const Name& name, RBNode<T>** new_node) {
    RBNode<T>* parent = NULLNODE;
    RBNode<T>* current = root_;

    int order = -1;
    while (current != NULLNODE) {
        parent = current;

        NameComparisonResult compare_result = name.compare(current->name_);
        NameComparisonResult::NameRelation relation =
            compare_result.getRelation();
        if (relation == NameComparisonResult::EQUAL) {
            if (new_node) {
                *new_node = current;
            }
            // if the node is non-ternimal, it does not exist, so we return 0
            return (current->is_shadow_ ? 0 : 1);
        } else {
            int common_label_count = compare_result.getCommonLabels();
            if (common_label_count == 1) {
                order = compare_result.getOrder();
                current = order < 0 ? current->left_ : current->right_;
            } else {
                // insert sub domain to sub tree
                if (relation == NameComparisonResult::SUBDOMAIN) {
                    if (NULL == current->down_) {
                        current->setDownTree(new RBTree());
                    }
                    return (current->down_->insert(name - current->name_,
                                                   new_node));
                } else {
                    // for super domain or has common label domain, create
                    // common node first then insert current name and new name
                    // into the sub tree
                    Name common_ancestor = name.split(
                        name.getLabelCount() - common_label_count,
                        common_label_count);
                    Name sub_name = current->name_ - common_ancestor;
                    current->name_ = common_ancestor;
                    RBTree<T>* down_old = current->down_;
                    current->setDownTree(new RBTree<T>());
                    RBNode<T>* sub_root;
                    current->down_->insert(sub_name, &sub_root);

                    current->cloneDNSData(*sub_root);
                    sub_root->setDownTree(down_old);
                    sub_root->name_ = sub_name;
                    current->is_shadow_ = true;

                    // if insert name is the super domain of current node, no
                    // need insert again otherwise insert it into the down
                    // tree.
                    if (name.getLabelCount() == common_label_count) {
                        *new_node = current;
                        current->is_shadow_ = false;
                        return (0);
                    } else {
                        return (current->down_->insert(name - common_ancestor,
                                                       new_node));
                    }
                }
            }
        }
    }

    RBNode<T>* node = new RBNode<T>(name, NULLNODE);
    node->parent_ = parent;
    if (parent == NULLNODE) {
        root_ = node;
    } else if (order < 0) {
        parent->left_ = node;
    } else {
        parent->right_ = node;
    }

    insertRebalance(node);
    if (new_node) {
        *new_node = node;
    }
    ++node_count_;
    return (0);
}

template <typename T>
void
RBTree<T>::insertRebalance(RBNode<T>* node) {
    RBNode<T>* uncle;

    while (node->parent_->color_ == RED) {
        if (node->parent_ == node->parent_->parent_->left_) {
            uncle = node->parent_->parent_->right_;

            if (uncle->color_ == RED) {
                node->parent_->color_ = BLACK;
                uncle->color_ = BLACK;
                node->parent_->parent_->color_ = RED;
                node = node->parent_->parent_;
            } else {
                if (node == node->parent_->right_) {
                    node = node->parent_;
                    leftRotate(node);
                }

                node->parent_->color_ = BLACK;
                node->parent_->parent_->color_ = RED;

                rightRotate(node->parent_->parent_);
            }
        } else {
            uncle = node->parent_->parent_->left_;

            if (uncle->color_ == RED) {
                node->parent_->color_ = BLACK;
                uncle->color_ = BLACK;
                node->parent_->parent_->color_ = RED;
                node = node->parent_->parent_;
            } else {
                if (node == node->parent_->left_) {
                    node = node->parent_;
                    rightRotate(node);
                }

                node->parent_->color_ = BLACK;
                node->parent_->parent_->color_ = RED;

                leftRotate(node->parent_->parent_);
            }
        }
    }

    root_->color_ = BLACK;
}

template <typename T>
RBNode<T>*
RBTree<T>::leftRotate(RBNode<T>* p) {
    RBNode<T>* c = p->right_;

    p->right_ = c->left_;

    if (c->left_ != NULLNODE) {
        c->left_->parent_ = p;
    }

    c->parent_ = p->parent_;

    if (p->parent_ == NULLNODE) {
        root_ = c;
    } else if (p == p->parent_->left_) {
        p->parent_->left_ = c;
    } else {
        p->parent_->right_ = c;
    }

    c->left_ = p;
    p->parent_ = c;

    return (c);
}

template <typename T>
RBNode<T>*
RBTree<T>::rightRotate(RBNode<T>* p) {
    RBNode<T>* c = p->left_;

    p->left_ = c->right_;

    if (c->right_ != NULLNODE) {
        c->right_->parent_ = p;
    }

    c->parent_ = p->parent_;

    if (p->parent_ == NULLNODE) {
        root_ = c;
    } else if (p == p->parent_->left_) {
        p->parent_->left_ = c;
    } else {
        p->parent_->right_ = c;
    }

    c->right_ = p;
    p->parent_ = c;

    return (c);
}


template <typename T>
int
RBTree<T>::erase(const Name& name) {
    RBNode<T>* node;
    RBTree<T>* tree;
    if (findHelper(name, &tree, &node) != RBTree<T>::EXACTMATCH) {
        return (1);
    }

    // cannot delete non terminal
    if (node->down_ != NULL) {
        return (1);
    }

    tree->eraseNode(node);
    // merge down to up
    if (tree->node_count_ == 1 && tree->up_ != NULL &&
        tree->up_->is_shadow_) {
        RBNode<T>* up = tree->up_;
        Name merged_name = tree->root_->name_.concatenate(up->name_);
        tree->root_->cloneDNSData(*up);
        up->setDownTree(tree->root_->down_);
        tree->root_->setDownTree(NULL);
        up->name_ = merged_name;
        up->is_shadow_ = false;
        delete tree;
    } else if (tree->node_count_ == 0 && tree->up_) { // delete empty tree
        tree->up_->setDownTree(NULL);
        delete tree;
    }

    return (0);
}


template <typename T>
void
RBTree<T>::eraseNode(RBNode<T> *node) {
    RBNode<T>* y = NULLNODE;
    RBNode<T>* x = NULLNODE;

    if (node->left_ == NULLNODE || node->right_ == NULLNODE) {
        y = node;
    } else {
        y = node->successor();
    }

    if (y->left_ != NULLNODE) {
        x = y->left_;
    } else {
        x = y->right_;
    }

    x->parent_ = y->parent_;

    if (y->parent_ == NULLNODE) {
        root_ = x;
    } else if (y == y->parent_->left_) {
        y->parent_->left_ = x;
    } else {
        y->parent_->right_ = x;
    }

    if (y != node) {
        y->cloneDNSData(*node);
        node->setDownTree(y->down_);
        y->down_ = NULL;
    }

    if (y->color_ == BLACK) {
        deleteRebalance(x);
    }

    y->left_ = NULL;
    y->right_ = NULL;
    delete y;
    --node_count_;
}

template <typename T>
void
RBTree<T>::deleteRebalance(RBNode<T>* node) {
    RBNode<T>* w = NULLNODE;

    while (node != root_ && node->color_ == BLACK) {
        if (node == node->parent_->left_) {
            w = node->parent_->right_;

            if (w->color_ == RED) {
                w->color_ = BLACK;
                node->parent_->color_ = RED;
                leftRotate(node->parent_);
                w = node->parent_->right_;
            }

            if (w->left_->color_ == BLACK && w->right_->color_ == BLACK) {
                w->color_ = RED;
                node = node->parent_;
            } else {
                if (w->right_->color_ == BLACK) {
                    w->left_->color_ = BLACK;
                    w->color_ = RED;
                    rightRotate(w);
                    w = node->parent_->right_;
                }

                w->color_ = node->parent_->color_;
                node->parent_->color_ = BLACK;
                w->right_->color_ = BLACK;
                leftRotate(node->parent_);
                node = root_;
            }
        } else {
            w = node->parent_->left_;
            if (w->color_ == RED) {
                w->color_ = BLACK;
                node->parent_->color_ = RED;
                rightRotate(node->parent_);
                w = node->parent_->left_;
            }

            if (w->right_->color_ == BLACK && w->left_->color_ == BLACK) {
                w->color_ = RED;
                node = node->parent_;
            } else {
                if (w->left_->color_ == BLACK) {
                    w->right_->color_ = BLACK;
                    w->color_ = RED;
                    leftRotate(w);
                    w = node->parent_->left_;
                }
                w->color_ = node->parent_->color_;
                node->parent_->color_ = BLACK;
                w->left_->color_ = BLACK;
                rightRotate(node->parent_);
                node = root_;
            }
        }
    }

    node->color_ = BLACK;
}

#define INDNET(depth) do { \
    int i = 0; \
    for (; i < (depth) * 5; ++i) { \
        std::cout << " "; \
    } \
} while(0)

template <typename T>
void
RBTree<T>::printTree(int depth) const {
    INDNET(depth);
    std::cout << "tree has node " << node_count_ << "\n";
    printTreeHelper(root_, depth);
}

template <typename T>
void
RBTree<T>::printTreeHelper(RBNode<T>* node, int depth) const {
    INDNET(depth);
    std::cout << node->name_.toText() << " ("
              << ((node->color_ == BLACK) ? "black" : "red") << ")\n";
    std::cout << ((node->isNonterminal()) ? "[non-terminal] \n" : "\n");
    if (node->down_) {
        assert(node->down_->up_ == node);
        INDNET(depth + 1);
        std::cout << "begin down from "<< node->name_.toText() << "\n";
        node->down_->printTree(depth + 1);
        INDNET(depth + 1);
        std::cout << "end down from" << node->name_.toText() <<"\n";
    }

    if (node->left_ != NULLNODE) {
        printTreeHelper(node->left_, depth + 1);
    } else {
        INDNET(depth + 1);
        std::cout << "NULL\n";
    }

    if (node->right_ != NULLNODE) {
        printTreeHelper(node->right_, depth + 1);
    } else {
        INDNET(depth + 1);
        std::cout << "NULL\n";
    }
}

}
}

#endif  // _RBTREE_H

// Local Variables: 
// mode: c++
// End: 

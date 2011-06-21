#ifndef CWCOLLISIONRECTKDTREE_H
#define CWCOLLISIONRECTKDTREE_H

//Qt includes
#include <QRectF>
class QPainter;

/**
  \brief This class is used to accelerate the text rendering by
  detecting rectangle collisions.
  */
class cwCollisionRectKdTree
{
public:
    cwCollisionRectKdTree();

    void clear();
   bool addRect(const QRect& rectangle);

   void paintTree(QPainter* painter, const QRect& windowSize);

private:
    class Node {
    public:
        Node();
        Node(QRect rectangle);
        ~Node() {
            if(LeftChild != NULL) { delete LeftChild; }
            if(RightChild != NULL) { delete RightChild; }
        }

        Node* LeftChild;
        Node* RightChild;
        QRect Rectangle; //only for the leaf nodes

        bool isLeaf() const { return LeftChild == NULL && RightChild == NULL; }
    };

    Node* Root;

    bool addRect(const QRect& rectangle, int depth, Node* root);

    void paintTree(QPainter* painter, QRect windowSize, int depth, Node* root);

};

#endif // CWCOLLISIONRECTKDTREE_H

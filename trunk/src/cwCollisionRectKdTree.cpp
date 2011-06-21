//Our includes
#include "cwCollisionRectKdTree.h"

//Qt includes
#include <QPainter>

cwCollisionRectKdTree::cwCollisionRectKdTree()
{
    Root = new Node();
    clear();
}

/**
  \brief This clears all the data from the kDTree
  */
void cwCollisionRectKdTree::clear() {
    delete Root;
    Root = new Node();
    Root->LeftChild = NULL;
    Root->RightChild = NULL;
}

/**
  \brief This adds a rectangle to the kDtree

  If the rectangle doesn't overlap any other rectangles, then
  this returns true, else the rectangle isn't added and this returns false.

  \param rectangle - The rectangle that'll be searched
  */
bool cwCollisionRectKdTree::addRect(const QRect& rectangle) {
    //Search the tree
    return addRect(rectangle, 0, Root);
}

/**
  \brief Creates an empty node
  */
cwCollisionRectKdTree::Node::Node() {
    LeftChild = NULL;
    RightChild = NULL;
}

/**
  \brief Creates a leaf node
  */
cwCollisionRectKdTree::Node::Node(QRect rectangle) {
    LeftChild = NULL;
    RightChild = NULL;
    Rectangle = rectangle;
}

/**
  Helper function to addRect
  */
bool cwCollisionRectKdTree::addRect(const QRect& rectangle, int depth, Node* root) {
    if(root->isLeaf()) {
        if(root->Rectangle.isNull()) {
            //Found a place to add the rectangle
            root->Rectangle = rectangle;
            return true;
        }
    }

    if(root->Rectangle.intersects(rectangle)) {
        //The rectangle overlap, can't add the rectangle
        return false;
    }

   //Select axis based on depth so that axis cycles through all valid values
   int axis = depth % 4; //divid by 4 because this is 2D range search problem

   //The heart of the kd-tree search, this switches the axises
   int leftValue;
   int rightValue;
   int axisValue;
   switch(axis) {
   case 0: //Left
       leftValue = rectangle.left();
       rightValue = rectangle.right();
       axisValue = root->Rectangle.left();
       break;
   case 1: //Top
       leftValue = rectangle.top();
       rightValue = rectangle.bottom();
       axisValue = root->Rectangle.top();
       break;
   case 2: //Right
       leftValue = rectangle.left();
       rightValue = rectangle.right();
       axisValue = root->Rectangle.right();
       break;
   case 3: //Bottom
       leftValue = rectangle.top();
       rightValue = rectangle.bottom();
       axisValue = root->Rectangle.bottom();
       break;
   }

   //If the rectangle is to the left
 // if(rectangleValue == root->Location) { return false; }
   if(leftValue < axisValue) {
       if(root->LeftChild != NULL) {
           //Search to the left
           return addRect(rectangle, depth + 1, root->LeftChild);
       } else {
           //Add a leaf to the left
           Node* leftNode = new Node(rectangle);
           root->LeftChild = leftNode;
           return true;
       }
  }

   if(rightValue >= axisValue) {
       //The rectangle is on the right
       if(root->RightChild != NULL) {
           //Search to the right
           return addRect(rectangle, depth + 1, root->RightChild);
       } else {
           //Add to the right
           Node* rightNode = new Node(rectangle);
           root->RightChild = rightNode;
           return true;
       }
   }

   return false;
}


void cwCollisionRectKdTree::paintTree(QPainter* painter, const QRect& windowSize) {
    painter->save();
    paintTree(painter, windowSize, 0, Root);
    painter->restore();
}

void cwCollisionRectKdTree::paintTree(QPainter* painter, QRect windowSize, int depth, Node* root) {
    if(root == NULL) { return; }
    int axis = depth % 4; //divid by 4 because this is 2D range search problem

    QRect windowSizeLeft = windowSize;
    QRect windowSizeRight = windowSize;

    //The heart of the kd-tree search, this switches the axises
    switch(axis) {
    case 0: //Left-
        painter->setPen(Qt::gray);
        painter->drawLine(root->Rectangle.left(), windowSize.top(), root->Rectangle.left(), windowSize.bottom());
        windowSizeLeft.setRight(root->Rectangle.left());
        windowSizeRight.setLeft(root->Rectangle.left());
        break;
    case 1: //Top
        painter->setPen(Qt::darkCyan);
        painter->drawLine(windowSize.left(), root->Rectangle.top(), windowSize.right(), root->Rectangle.top());
        windowSizeLeft.setBottom(root->Rectangle.top());
        windowSizeRight.setTop(root->Rectangle.top());
        break;
    case 2: //Right
        painter->setPen(Qt::lightGray);
        painter->drawLine(root->Rectangle.right(), windowSize.top(), root->Rectangle.right(), windowSize.bottom());
        windowSizeLeft.setRight(root->Rectangle.right());
        windowSizeRight.setLeft(root->Rectangle.right());
        break;
    case 3: //Bottom
        painter->setPen(Qt::cyan);
        painter->drawLine(windowSize.left(), root->Rectangle.bottom(), windowSize.right(), root->Rectangle.bottom());
        windowSizeLeft.setBottom(root->Rectangle.bottom());
        windowSizeRight.setTop(root->Rectangle.bottom());
        break;
    }

   // painter->setPen(QPen());
   // painter->drawText(windowSize.bottomLeft(), QString("%1").arg(depth));

    painter->setPen(QPen(Qt::red));
    painter->drawRect(root->Rectangle);
    paintTree(painter, windowSizeLeft, depth + 1, root->LeftChild);
    paintTree(painter, windowSizeRight, depth + 1, root->RightChild);
}

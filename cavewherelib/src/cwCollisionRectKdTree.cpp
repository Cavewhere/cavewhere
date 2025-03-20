/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCollisionRectKdTree.h"

//Qt includes
#include <QPainter>

cwCollisionRectKdTree::cwCollisionRectKdTree()
{
    Root = new Node();
    clear();
}

cwCollisionRectKdTree::~cwCollisionRectKdTree()
{
    delete Root;
}

/**
  \brief This clears all the data from the kDTree
  */
void cwCollisionRectKdTree::clear() {
    delete Root;
    Root = new Node();
    Root->LeftChild = nullptr;
    Root->RightChild = nullptr;
}

/**
  \brief This adds a rectangle to the kDtree

  If the rectangle doesn't overlap any other rectangles, then
  this returns true, else the rectangle isn't added and this returns false.

  \param rectangle - The rectangle that'll be searched
  */
bool cwCollisionRectKdTree::addRect(const QRect& rectangle) {
    //If the rectangle can be added
    if(rectIntersects(rectangle, 0, Root)) {
        //We found an inersecting rectangle with rectangle.  Can't add the rectangle
        //Return false
        return false;
    }

    //Add the rectangle, the rectangle is clear to be added
    addRect(rectangle, 0, Root);
    return true;
}

/**
  \brief Creates an empty node
  */
cwCollisionRectKdTree::Node::Node() {
    LeftChild = nullptr;
    RightChild = nullptr;
}

/**
  \brief Creates a leaf node
  */
cwCollisionRectKdTree::Node::Node(QRect rectangle) {
    LeftChild = nullptr;
    RightChild = nullptr;
    Rectangle = rectangle;
}

/**
  \brief This searches for collisions with the rectangle in the kd-Tree

  If the rectangle overlaps another rectangle, this return true, else this returns false
  */
bool cwCollisionRectKdTree::rectIntersects(const QRect& rectangle, int depth, Node* root) {
    if(root == nullptr) { return false; } //No rectangles to test, rectangle doesn't intesect with this node
    if(root->isLeaf()) {
        //Do the intersection test
        return root->Rectangle.intersects(rectangle);
    }

    //Select axis based on depth so that axis cycles through all valid values
    int axis = depth % 2; //divid by 4 because this is 2D range search problem

    //int leftValue = value(rectangle, axis);
    int rightValue = oppositeValue(rectangle, axis);
    int axisValue = root->value(axis);

//    if(leftValue < axisValue &&
//            axisValue <= rightValue) {
//        //Rectangle goes between the splits, search left and right nodes
//        return rectIntersects(rectangle, depth + 1, root->LeftChild) ||
//                rectIntersects(rectangle, depth + 1, root->RightChild);

  if(rightValue < axisValue) {
        //Completely clears the left side of the axis
        return rectIntersects(rectangle, depth + 1, root->LeftChild);
    } else {
        //Completely clears the right side of the axis
        return rectIntersects(rectangle, depth + 1, root->LeftChild) ||
                rectIntersects(rectangle, depth + 1, root->RightChild);
    }

}

/**
  Helper function to addRect
  */
void cwCollisionRectKdTree::addRect(const QRect& rectangle, int depth, Node* root) {

    //Select axis based on depth so that axis cycles through all valid values
    int axis = depth % 2; //divid by 4 because this is 2D range search problem

    if(root->isLeaf()) {
        if(root->Rectangle.isNull()) {
            //Found a place to add the rectangle
            root->Rectangle = rectangle;
            return;
        } else {
            //Split the this leaf node, so it's not a leaf node anymore

            Node* node1 = new Node(rectangle);
            Node* node2 = new Node(root->Rectangle);

            if(node1->value(axis) < node2->value(axis)) {
                root->LeftChild = node1;
                root->RightChild = node2;
            } else {
                root->LeftChild = node2;
                root->RightChild = node1;
            }
            root->Rectangle = root->RightChild->Rectangle;
            return;
        }
    }

    int rectangleValue = value(rectangle, axis);
    int axisValue = root->value(axis);

    if(rectangleValue < axisValue) {
        //Search to the left
        addRect(rectangle, depth + 1, root->LeftChild);
    } else {
        //Search to the right
        addRect(rectangle, depth + 1, root->RightChild);
    }

    return;

//   //The heart of the kd-tree search, this switches the axises
//   int leftValue;
//   int rightValue;
//   int axisValue;
//   switch(axis) {
//   case 0: //Left
//       leftValue = rectangle.left();
//       rightValue = rectangle.right();
//       axisValue = root->Rectangle.left();
//       break;
//   case 1: //Top
//       leftValue = rectangle.top();
//       rightValue = rectangle.bottom();
//       axisValue = root->Rectangle.top();
//       break;
//   case 2: //Right
//       leftValue = rectangle.left();
//       rightValue = rectangle.right();
//       axisValue = root->Rectangle.right();
//       break;
//   case 3: //Bottom
//       leftValue = rectangle.top();
//       rightValue = rectangle.bottom();
//       axisValue = root->Rectangle.bottom();
//       break;
//   }

//   //If the rectangle is to the left
// // if(rectangleValue == root->Location) { return false; }
//   if(leftValue < axisValue) {
//       if(root->LeftChild != nullptr) {
//           //Search to the left
//           return addRect(rectangle, depth + 1, root->LeftChild);
//       } else {
//           //Add a leaf to the left
//           Node* leftNode = new Node(rectangle);
//           root->LeftChild = leftNode;
//           return true;
//       }
//  }

//   if(rightValue >= axisValue) {
//       //The rectangle is on the right
//       if(root->RightChild != nullptr) {
//           //Search to the right
//           return addRect(rectangle, depth + 1, root->RightChild);
//       } else {
//           //Add to the right
//           Node* rightNode = new Node(rectangle);
//           root->RightChild = rightNode;
//           return true;
//       }
//   }

//   return false;
}


void cwCollisionRectKdTree::paintTree(QPainter* painter, const QRect& windowSize) {
    painter->save();
    paintTree(painter, windowSize, 0, Root);
    painter->restore();
}

void cwCollisionRectKdTree::paintTree(QPainter* painter, QRect windowSize, int depth, Node* root) {
    if(root == nullptr) { return; }

    if(root->isLeaf()) {
        painter->setPen(QPen(Qt::red));
        painter->drawRect(root->Rectangle);
        return;
    }

    int axis = depth % 2; //divid by 4 because this is 2D range search problem

    QRect windowSizeLeft = windowSize;
    QRect windowSizeRight = windowSize;

    QPen pen;
    pen.setWidthF(2.0);

    //The heart of the kd-tree search, this switches the axises
    switch(axis) {
    case 0: //Left-
        pen.setColor(Qt::gray);
        painter->setPen(pen);
        painter->drawLine(root->Rectangle.left(), windowSize.top(), root->Rectangle.left(), windowSize.bottom());
        windowSizeLeft.setRight(root->Rectangle.left());
        windowSizeRight.setLeft(root->Rectangle.left());
        break;
    case 1: //Top
        pen.setColor(Qt::darkCyan);
        painter->setPen(pen);
        painter->drawLine(windowSize.left(), root->Rectangle.top(), windowSize.right(), root->Rectangle.top());
        windowSizeLeft.setBottom(root->Rectangle.top());
        windowSizeRight.setTop(root->Rectangle.top());
        break;
    case 2: //Right
        pen.setColor(Qt::lightGray);
        painter->setPen(pen);
        painter->drawLine(root->Rectangle.right(), windowSize.top(), root->Rectangle.right(), windowSize.bottom());
        windowSizeLeft.setRight(root->Rectangle.right());
        windowSizeRight.setLeft(root->Rectangle.right());
        break;
    case 3: //Bottom
        pen.setColor(Qt::cyan);
        painter->setPen(pen);
        painter->drawLine(windowSize.left(), root->Rectangle.bottom(), windowSize.right(), root->Rectangle.bottom());
        windowSizeLeft.setBottom(root->Rectangle.bottom());
        windowSizeRight.setTop(root->Rectangle.bottom());
        break;
    }

   // painter->setPen(QPen());
   // painter->drawText(windowSize.bottomLeft(), QString("%1").arg(depth));

    paintTree(painter, windowSizeLeft, depth + 1, root->LeftChild);
    paintTree(painter, windowSizeRight, depth + 1, root->RightChild);
}

/**
  \brief Gets the value at a specific axis

  \param axis
  */
float cwCollisionRectKdTree::value(const QRect& Rectangle, int axis) {
    switch(axis) {
    case 0: //Left
        return Rectangle.left();
    case 1: //Top
        return Rectangle.top();
//    case 2: //Right
//        return Rectangle.right();
//    case 3: //Bottom
//        return Rectangle.bottom();
    }
    Q_ASSERT(false);
    return 0.0;
}

/**
  \brief This is similar to value() but this gets the opposite value

  For example if value returns the left value, this returns the right value
  */
float cwCollisionRectKdTree::oppositeValue(const QRect& Rectangle, int axis) {
    switch(axis) {
    case 0: //Left opposite
        return Rectangle.right();
    case 1: //Top opposite
        return Rectangle.bottom();
//    case 2: //Right opposite
//        return Rectangle.left();
//    case 3: //Bottom opposite
//        return Rectangle.top();
    }
    Q_ASSERT(false);
    return 0.0;
}

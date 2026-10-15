# Math API

`Vector4`, `Matrix4`, `Color`.

## Vector4 Module

**Construction:**
```js
let vec = new Vector4(x, y, z, w);
```
```js
let test = new Vector4(0.0, 0.0, 0.0, 1.0);
```

**Properties:**

* `x`, `y`, `z`, `w` - Vector components.

**Methods:**

* `norm()` - Get vector length/norm.
* `dot(vec2)` - Calculate vector dot product.
* `cross(vec2)` - Calculate vector cross product.
* `distance(vec2)` - Calculate vector distances.
* `distance2(vec2)` - Calculate vector squared distances.
* `add(vec2)` - Returns a new vector, the sum of this vector and `vec2`.
* `sub(vec2)` - Returns a new vector, the difference of this vector and `vec2`.
* `mul(vec2)` - Returns a new vector, the component-wise product of this vector and `vec2`.
* `div(vec2)` - Returns a new vector, the component-wise division of this vector by `vec2`.
* `equals(vec2)` - Returns true if every component matches `vec2`.

## Matrix4 Module

**Construction:**
```js
let mat = new Matrix4(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16)
```
or
```js
let mat = new Matrix4(); // No arguments means identity matrix.
```

**Properties:**

* `length` - 16.

**Methods:**

* `toArray()` - Returns an array from the matrix.
* `fromArray()` - Puts the first 16 numbers of the array as the matrix elements.
* `clone()` - Create a new clone of the given matrix.
* `copy(mat)` - Copy the matrix contents to the given matrix.
* `transpose()` - Transpose matrix.
* `invert()` - Calculate matrix inverse.
* `identity()` - Put matrix identity.
* `mul(mat)` - Returns a new matrix, the product of this matrix and `mat`.
* `equals(mat)` - Returns true if every element matches `mat`.

Matrix4 components can also be accessed as an array: `mat[n]` from 0 to 15.

## Color module

* `let col = Color.new(r, g, b, *a*)` - Returns a color object from the specified RGB(A) parameters.
* `let r = Color.getR(col)` - Get red intensity of the color.
* `let g = Color.getG(col)` - Get green intensity of the color.
* `let b = Color.getB(col)` - Get blue intensity of the color.
* `let a = Color.getA(col)` - Get alpha intensity of the color.
* `Color.setR(col, r)` - Set red intensity of the color.
* `Color.setG(col, g)` - Set green intensity of the color.
* `Color.setB(col, b)` - Set blue intensity of the color.
* `Color.setA(col, a)` - Set alpha intensity of the color.

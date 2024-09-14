
## TAperture::EOp::OVERLAY

### Alpha Blending Formula
Alpha blending is typically performed using the following formulas:

#### Color Blending:
\[
C_{\text{result}} = C_{\text{top}} \cdot A_{\text{top}} + C_{\text{bottom}} \cdot (1 - A_{\text{top}})
\]
Where:
- \( C_{\text{result}} \) is the resulting color (for each channel: R, G, B).
- \( C_{\text{top}} \) is the color of the top shape.
- \( C_{\text{bottom}} \) is the color of the bottom shape.
- \( A_{\text{top}} \) is the alpha value of the top shape (in the range [0, 1]).
- \( A_{\text{bottom}} \) is the alpha value of the bottom shape, which is implicitly included through transparency.

#### Alpha Blending:
The resulting alpha value can be computed as:
\[
A_{\text{result}} = A_{\text{top}} + A_{\text{bottom}} \cdot (1 - A_{\text{top}})
\]
This formula ensures that the transparency of the top shape affects how much of the bottom shape is visible. If the top shape is fully transparent (\(A_{\text{top}} = 0\)), the bottom shape is fully visible. If the top shape is fully opaque (\(A_{\text{top}} = 1\)), the bottom shape is fully obscured.

### Example Calculation:
Let’s say we have two overlapping shapes:

- **ShapeBottom**: (R=0.3, G=0.5, B=0.7, A=0.8)
- **ShapeTop**: (R=0.9, G=0.4, B=0.2, A=0.5)

For a pixel where they overlap, we can calculate the result as follows:

- **Blending Colors**:
  - \( C_{\text{result},R} = 0.9 \times 0.5 + 0.3 \times (1 - 0.5) = 0.45 + 0.15 = 0.6 \)
  - \( C_{\text{result},G} = 0.4 \times 0.5 + 0.5 \times (1 - 0.5) = 0.2 + 0.25 = 0.45 \)
  - \( C_{\text{result},B} = 0.2 \times 0.5 + 0.7 \times (1 - 0.5) = 0.1 + 0.35 = 0.45 \)

- **Blending Alpha**:
  - \( A_{\text{result}} = 0.5 + 0.8 \times (1 - 0.5) = 0.5 + 0.4 = 0.9 \)

- **Resulting Pixel**: The resulting pixel color and alpha would be \( C_{\text{result}} = (R=0.6, G=0.45, B=0.45, A=0.9) \).

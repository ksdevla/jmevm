package javax.microedition.lcdui.game;

import java.util.Vector;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

/**
 * A grid of cells, each showing a tile of an image (1 and up), nothing (0),
 * or an animated tile (-1 and down) that stands for a static tile the
 * application changes.
 */
public class TiledLayer extends Layer {
    private final int columns, rows;
    private final int[] cells;
    private Image image;
    private int tileW, tileH, tileColumns, tiles;
    private final Vector animated = new Vector();   /* Integer static tiles; -1 is index 0 */

    public TiledLayer(int columns, int rows, Image image, int tileWidth, int tileHeight) {
        super(columns * tileWidth, rows * tileHeight);
        if (columns <= 0 || rows <= 0) {
            throw new IllegalArgumentException("grid of " + columns + "x" + rows);
        }
        this.columns = columns;
        this.rows = rows;
        cells = new int[columns * rows];
        setTiles(image, tileWidth, tileHeight);
    }

    private void setTiles(Image img, int tw, int th) {
        if (img == null) {
            throw new NullPointerException();
        }
        if (tw <= 0 || th <= 0 || img.getWidth() % tw != 0 || img.getHeight() % th != 0) {
            throw new IllegalArgumentException("tiles of " + tw + "x" + th + " do not divide the image");
        }
        image = img;
        tileW = tw;
        tileH = th;
        tileColumns = img.getWidth() / tw;
        tiles = tileColumns * (img.getHeight() / th);
    }

    public void setStaticTileSet(Image image, int tileWidth, int tileHeight) {
        int before = tiles;
        setTiles(image, tileWidth, tileHeight);
        width = columns * tileWidth;
        height = rows * tileHeight;
        if (tiles < before) {   /* fewer tiles: the cells and animated tiles start over */
            for (int i = 0; i < cells.length; i++) {
                cells[i] = 0;
            }
            animated.removeAllElements();
        }
    }

    public int createAnimatedTile(int staticTileIndex) {
        checkStatic(staticTileIndex);
        animated.addElement(new Integer(staticTileIndex));
        return -animated.size();
    }

    private void checkStatic(int index) {
        if (index < 0 || index > tiles) {
            throw new IndexOutOfBoundsException("tile " + index);
        }
    }

    private void checkAnimated(int index) {
        if (index >= 0 || -index > animated.size()) {
            throw new IndexOutOfBoundsException("animated tile " + index);
        }
    }

    public void setAnimatedTile(int animatedTileIndex, int staticTileIndex) {
        checkAnimated(animatedTileIndex);
        checkStatic(staticTileIndex);
        animated.setElementAt(new Integer(staticTileIndex), -animatedTileIndex - 1);
    }

    public int getAnimatedTile(int animatedTileIndex) {
        checkAnimated(animatedTileIndex);
        return ((Integer) animated.elementAt(-animatedTileIndex - 1)).intValue();
    }

    private void checkTile(int tile) {
        if (tile < 0) {
            checkAnimated(tile);
        } else {
            checkStatic(tile);
        }
    }

    public void setCell(int col, int row, int tileIndex) {
        if (col < 0 || col >= columns || row < 0 || row >= rows) {
            throw new IndexOutOfBoundsException("cell " + col + "," + row);
        }
        checkTile(tileIndex);
        cells[row * columns + col] = tileIndex;
    }

    public int getCell(int col, int row) {
        if (col < 0 || col >= columns || row < 0 || row >= rows) {
            throw new IndexOutOfBoundsException("cell " + col + "," + row);
        }
        return cells[row * columns + col];
    }

    public void fillCells(int col, int row, int numCols, int numRows, int tileIndex) {
        if (numCols < 0 || numRows < 0) {
            throw new IllegalArgumentException("region of " + numCols + "x" + numRows);
        }
        if (col < 0 || row < 0 || col + numCols > columns || row + numRows > rows) {
            throw new IndexOutOfBoundsException("region outside the grid");
        }
        checkTile(tileIndex);
        for (int r = row; r < row + numRows; r++) {
            for (int c = col; c < col + numCols; c++) {
                cells[r * columns + c] = tileIndex;
            }
        }
    }

    public final int getCellWidth() {
        return tileW;
    }

    public final int getCellHeight() {
        return tileH;
    }

    public final int getColumns() {
        return columns;
    }

    public final int getRows() {
        return rows;
    }

    /** The static tile a cell shows (0: none). */
    private int tileOf(int cell) {
        return cell >= 0 ? cell : ((Integer) animated.elementAt(-cell - 1)).intValue();
    }

    public final void paint(Graphics g) {
        if (!visible) {
            return;
        }
        int c1 = Math.max(0, (g.getClipX() - x) / tileW), r1 = Math.max(0, (g.getClipY() - y) / tileH);
        int c2 = Math.min(columns - 1, (g.getClipX() + g.getClipWidth() - x - 1) / tileW);
        int r2 = Math.min(rows - 1, (g.getClipY() + g.getClipHeight() - y - 1) / tileH);
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                int tile = tileOf(cells[r * columns + c]);
                if (tile > 0) {
                    int t = tile - 1;
                    g.drawRegion(image, (t % tileColumns) * tileW, (t / tileColumns) * tileH, tileW, tileH,
                            Sprite.TRANS_NONE, x + c * tileW, y + r * tileH, Graphics.TOP | Graphics.LEFT);
                }
            }
        }
    }

    /** For collisions: the cell's static tile at screen pixel (sx, sy), 0 outside. */
    int cellAt(int sx, int sy) {
        int c = (sx - x) / tileW, r = (sy - y) / tileH;
        if (sx < x || sy < y || c >= columns || r >= rows) {
            return 0;
        }
        return tileOf(cells[r * columns + c]);
    }

    boolean opaqueAt(int sx, int sy) {
        int tile = cellAt(sx, sy);
        if (tile == 0) {
            return false;
        }
        int t = tile - 1;
        return Pixels.opaque(image, (t % tileColumns) * tileW + (sx - x) % tileW,
                (t / tileColumns) * tileH + (sy - y) % tileH);
    }
}

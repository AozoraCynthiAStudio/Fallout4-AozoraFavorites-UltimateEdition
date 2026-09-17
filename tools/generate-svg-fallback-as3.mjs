import fs from 'node:fs';
import path from 'node:path';

const root = path.resolve(import.meta.dirname, '..');
const iconDir = path.join(root, '图标');
const outputFile = path.join(root, 'src', 'as3', 'aozora', 'favorites', 'FallbackIconFactory.as');

const sources = {
    armor: 'armor.svg',
    clothing: 'clothing .svg',
    explosive: 'explosive.svg',
    fooddrink: 'FoodDrink.svg',
    footwear: 'footwear .svg',
    headwear: 'headwear .svg',
    heavy: 'heavy .svg',
    medicine: 'Medicine.svg',
    melee: 'melee.svg',
    pistol: 'pistol.svg',
    rifle: 'rifle.svg',
    utility: 'Utility.svg'
};

const categories = {
    pistol: 'Pistol', rifle: 'Rifle', heavy: 'Heavy', melee: 'Melee', explosive: 'Explosive',
    clothing: 'Clothing', armor: 'Armor', headwear: 'Headwear', footwear: 'Footwear',
    medicine: 'Medicine', fooddrink: 'FoodDrink', utility: 'Utility'
};

function number(value) {
    const n = Math.abs(value) < 0.0005 ? 0 : Number(value.toFixed(3));
    return String(n);
}

function parseViewBox(svg) {
    const match = svg.match(/viewBox\s*=\s*["']([^"']+)["']/i);
    if (!match) throw new Error('SVG has no viewBox');
    const values = match[1].trim().split(/[\s,]+/).map(Number);
    if (values.length !== 4 || values.some(v => !Number.isFinite(v)) || values[2] <= 0 || values[3] <= 0)
        throw new Error(`Invalid viewBox: ${match[1]}`);
    return values;
}

function transformFactory(viewBox) {
    const [, , width, height] = viewBox;
    const scale = Math.min(28 / width, 28 / height);
    const offsetX = (32 - width * scale) / 2;
    const offsetY = (32 - height * scale) / 2;
    return (x, y) => [offsetX + x * scale, offsetY + y * scale];
}

function tokenizePath(d) {
    const tokens = [];
    const pattern = /([AaCcHhLlMmQqSsTtVvZz])|([-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?)/g;
    let match;
    while ((match = pattern.exec(d))) tokens.push(match[1] ?? Number(match[2]));
    return tokens;
}

const counts = { M: 2, L: 2, H: 1, V: 1, C: 6, S: 4, Q: 4, T: 2, A: 7, Z: 0 };

function pathCommands(d, transform) {
    const tokens = tokenizePath(d);
    const commands = [];
    const data = [];
    let index = 0;
    let command = null;
    let x = 0, y = 0, startX = 0, startY = 0;
    let lastCubicX = 0, lastCubicY = 0, lastQuadraticX = 0, lastQuadraticY = 0;
    let previous = '';

    const addMove = (px, py) => {
        const [tx, ty] = transform(px, py); commands.push(1); data.push(tx, ty);
    };
    const addLine = (px, py) => {
        const [tx, ty] = transform(px, py); commands.push(2); data.push(tx, ty);
    };
    const addCurve = (c1x, c1y, c2x, c2y, px, py) => {
        const a = transform(c1x, c1y), b = transform(c2x, c2y), c = transform(px, py);
        commands.push(3); data.push(a[0], a[1], b[0], b[1], c[0], c[1]);
    };
    const resetControls = () => {
        lastCubicX = x; lastCubicY = y; lastQuadraticX = x; lastQuadraticY = y;
    };

    while (index < tokens.length) {
        if (typeof tokens[index] === 'string') command = tokens[index++];
        if (!command) throw new Error(`Path data starts with a number: ${d.slice(0, 40)}`);
        const upper = command.toUpperCase();
        if (upper === 'Z') {
            addLine(startX, startY); x = startX; y = startY; previous = 'Z'; command = null; continue;
        }
        const count = counts[upper];
        if (count === undefined) throw new Error(`Unsupported SVG command ${command}`);
        if (index + count > tokens.length || tokens.slice(index, index + count).some(v => typeof v === 'string'))
            throw new Error(`Incomplete SVG command ${command}`);
        const values = tokens.slice(index, index + count).map(Number); index += count;
        const relative = command === command.toLowerCase();
        const px = (v, base) => relative ? base + v : v;

        if (upper === 'M') {
            x = px(values[0], x); y = px(values[1], y); addMove(x, y); startX = x; startY = y;
            previous = 'M'; command = relative ? 'l' : 'L'; resetControls();
        } else if (upper === 'L') {
            x = px(values[0], x); y = px(values[1], y); addLine(x, y); previous = 'L'; resetControls();
        } else if (upper === 'H') {
            x = relative ? x + values[0] : values[0]; addLine(x, y); previous = 'H'; resetControls();
        } else if (upper === 'V') {
            y = relative ? y + values[0] : values[0]; addLine(x, y); previous = 'V'; resetControls();
        } else if (upper === 'C') {
            const c1x = px(values[0], x), c1y = px(values[1], y);
            const c2x = px(values[2], x), c2y = px(values[3], y);
            x = px(values[4], x); y = px(values[5], y); addCurve(c1x, c1y, c2x, c2y, x, y);
            lastCubicX = c2x; lastCubicY = c2y; previous = 'C'; lastQuadraticX = x; lastQuadraticY = y;
        } else if (upper === 'S') {
            const c1x = (previous === 'C' || previous === 'S') ? 2 * x - lastCubicX : x;
            const c1y = (previous === 'C' || previous === 'S') ? 2 * y - lastCubicY : y;
            const c2x = px(values[0], x), c2y = px(values[1], y);
            x = px(values[2], x); y = px(values[3], y); addCurve(c1x, c1y, c2x, c2y, x, y);
            lastCubicX = c2x; lastCubicY = c2y; previous = 'S'; lastQuadraticX = x; lastQuadraticY = y;
        } else if (upper === 'Q' || upper === 'T') {
            let qx, qy;
            if (upper === 'Q') { qx = px(values[0], x); qy = px(values[1], y); }
            else {
                qx = (previous === 'Q' || previous === 'T') ? 2 * x - lastQuadraticX : x;
                qy = (previous === 'Q' || previous === 'T') ? 2 * y - lastQuadraticY : y;
            }
            const endX = upper === 'Q' ? px(values[2], x) : px(values[0], x);
            const endY = upper === 'Q' ? px(values[3], y) : px(values[1], y);
            const c1x = x + (2 / 3) * (qx - x), c1y = y + (2 / 3) * (qy - y);
            const c2x = endX + (2 / 3) * (qx - endX), c2y = endY + (2 / 3) * (qy - endY);
            addCurve(c1x, c1y, c2x, c2y, endX, endY);
            lastQuadraticX = qx; lastQuadraticY = qy; x = endX; y = endY; previous = upper; lastCubicX = x; lastCubicY = y;
        } else if (upper === 'A') {
            // The supplied files use no true arc commands in their geometry.
            // Keep a safe endpoint fallback if a future SVG adds one.
            x = px(values[5], x); y = px(values[6], y); addLine(x, y); previous = 'A'; resetControls();
        }
    }
    return { commands, data };
}

function attr(tag, name, fallback = 0) {
    const match = tag.match(new RegExp(`${name}\\s*=\\s*["']([^"']+)["']`, 'i'));
    return match ? Number(match[1]) : fallback;
}

function shapes(svg, transform) {
    const result = [];
    const pathPattern = /<path\b[^>]*\bd\s*=\s*["']([^"']+)["'][^>]*>/gi;
    let match;
    while ((match = pathPattern.exec(svg))) {
        const parsed = pathCommands(match[1], transform);
        if (parsed.commands.length) result.push({ kind: 'path', ...parsed });
    }
    const rectPattern = /<rect\b([^>]*)\/?\s*>/gi;
    while ((match = rectPattern.exec(svg))) {
        const tag = match[1];
        const x = attr(tag, 'x'), y = attr(tag, 'y'), width = attr(tag, 'width'), height = attr(tag, 'height');
        const a = transform(x, y), b = transform(x + width, y + height);
        result.push({ kind: 'rect', x: a[0], y: a[1], width: b[0] - a[0], height: b[1] - a[1] });
    }
    const ellipsePattern = /<ellipse\b([^>]*)\/?\s*>/gi;
    while ((match = ellipsePattern.exec(svg))) {
        const tag = match[1];
        const cx = attr(tag, 'cx'), cy = attr(tag, 'cy'), rx = attr(tag, 'rx'), ry = attr(tag, 'ry');
        const a = transform(cx - rx, cy - ry), b = transform(cx + rx, cy + ry);
        result.push({ kind: 'ellipse', x: a[0], y: a[1], width: b[0] - a[0], height: b[1] - a[1] });
    }
    const circlePattern = /<circle\b([^>]*)\/?\s*>/gi;
    while ((match = circlePattern.exec(svg))) {
        const tag = match[1];
        const cx = attr(tag, 'cx'), cy = attr(tag, 'cy'), radius = attr(tag, 'r');
        const a = transform(cx - radius, cy - radius), b = transform(cx + radius, cy + radius);
        result.push({ kind: 'ellipse', x: a[0], y: a[1], width: b[0] - a[0], height: b[1] - a[1] });
    }
    return result;
}

function vectorLiteral(values) {
    return values.length ? values.map(number).join(', ') : '';
}

function renderShapes(name, svg) {
    const transform = transformFactory(parseViewBox(svg));
    const list = shapes(svg, transform);
    const lines = [`        private static function draw${name}(g:Graphics):void`, '        {'];
    for (const shape of list) {
        if (shape.kind === 'path') {
            lines.push(`            var commands:Vector.<int> = new <int>[${vectorLiteral(shape.commands)}];`);
            lines.push(`            var data:Vector.<Number> = new <Number>[${vectorLiteral(shape.data)}];`);
            lines.push('            g.beginFill(INK, 0.96);');
            lines.push('            g.drawPath(commands, data, "nonZero");');
            lines.push('            g.endFill();');
        } else if (shape.kind === 'rect') {
            lines.push(`            g.beginFill(INK, 0.96); g.drawRect(${number(shape.x)}, ${number(shape.y)}, ${number(shape.width)}, ${number(shape.height)}); g.endFill();`);
        } else {
            lines.push(`            g.beginFill(INK, 0.96); g.drawEllipse(${number(shape.x)}, ${number(shape.y)}, ${number(shape.width)}, ${number(shape.height)}); g.endFill();`);
        }
    }
    lines.push('        }', '');
    return lines;
}

const output = [
    'package aozora.favorites', '{', '    import flash.display.Graphics;', '    import flash.display.Sprite;', '',
    '    /** Build-generated from the 12 SVG files supplied in 图标/. */',
    '    public final class FallbackIconFactory', '    {', '        private static const INK:uint = 0xFFFFFF;', '',
    '        public static function create(type:String, value:String):Sprite', '        {',
    '            var holder:Sprite = new Sprite();', '            var art:Sprite = new Sprite();',
    '            var g:Graphics = art.graphics;', '            var category:String = normalize(type, value);',
    '            holder.mouseEnabled = false;', '            holder.mouseChildren = false;', '            holder.addChild(art);', '',
    '            if (category == "Pistol") drawPistol(g);', '            else if (category == "Rifle") drawRifle(g);',
    '            else if (category == "Heavy") drawHeavy(g);', '            else if (category == "Melee") drawMelee(g);',
    '            else if (category == "Explosive") drawExplosive(g);', '            else if (category == "Clothing") drawClothing(g);',
    '            else if (category == "Armor") drawArmor(g);', '            else if (category == "Headwear") drawHeadwear(g);',
    '            else if (category == "Footwear") drawFootwear(g);', '            else if (category == "Medicine") drawMedicine(g);',
    '            else if (category == "FoodDrink") drawFoodDrink(g);', '            else drawUtility(g);', '',
    '            art.scaleX = 0.5;', '            art.scaleY = 0.5;', '            return holder;', '        }', '',
    '        private static function normalize(type:String, value:String):String', '        {',
    '            var category:String = value ? value.toLowerCase() : "";',
    '            if (!category.length) return type == "服装" ? "Clothing" : (type == "药品" ? "Medicine" : (type == "武器" ? "Rifle" : "Utility"));',
    '            if (category == "pistol") return "Pistol";', '            if (category == "rifle" || category == "shotgun" || category == "ranged") return "Rifle";',
    '            if (category == "heavy") return "Heavy";', '            if (category == "melee") return "Melee";',
    '            if (category == "explosive" || category == "throwable") return "Explosive";', '            if (category == "clothing" || category == "outfit") return "Clothing";',
    '            if (category == "armor") return "Armor";', '            if (category == "headwear") return "Headwear";',
    '            if (category == "footwear") return "Footwear";', '            if (category == "medicine" || category == "aid") return "Medicine";',
    '            if (category == "fooddrink" || category == "food") return "FoodDrink";', '            return "Utility";',
    '        }', ''
];

for (const [key, file] of Object.entries(sources)) {
    const svg = fs.readFileSync(path.join(iconDir, file), 'utf8');
    output.push(...renderShapes(categories[key], svg));
}
output.push('    }', '}', '');
fs.writeFileSync(outputFile, output.join('\n'), 'utf8');
console.log(`GENERATED=${outputFile}`);
console.log(`BYTES=${fs.statSync(outputFile).size}`);

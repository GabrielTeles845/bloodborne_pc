import QtQuick

Canvas {
    id: icon
    property string kind: "home"
    property color strokeColor: "#e4e6e8"
    width: 25
    height: 25
    onKindChanged: requestPaint()
    onStrokeColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const c = getContext("2d")
        c.reset()
        // Paths use a 25 × 25 grid: scale them to the icon's size so smaller icons are not cut off,
        // keeping the on-screen stroke width.
        const s = Math.min(width, height) / 25
        c.scale(s, s)
        c.strokeStyle = strokeColor
        c.fillStyle = strokeColor
        c.lineWidth = 1.8 / s
        c.lineCap = "round"
        c.lineJoin = "round"
        function line(points) {
            c.beginPath(); c.moveTo(points[0], points[1])
            for (let i=2; i<points.length; i+=2) c.lineTo(points[i], points[i+1])
            c.stroke()
        }
        function box(x,y,w,h,r) { c.beginPath(); c.rect(x,y,w,h); c.stroke() }
        function circle(x,y,r) { c.beginPath(); c.arc(x,y,r,0,2*Math.PI); c.stroke() }
        if (kind === "home") {
            line([3,11,12.5,3,22,11]); line([5,10,5,22,20,22,20,10]); box(10,15,5,7,0)
        } else if (kind === "graphics") {
            box(2,4,21,15,2); line([9,22,16,22]); line([12.5,19,12.5,22])
        } else if (kind === "display") {
            box(3,3,19,19,2); line([6,17,18,17]); line([8,19,16,19])
        } else if (kind === "audio") {
            line([3,10,7,10,12,6,12,19,7,15,3,15,3,10]);
            c.beginPath(); c.arc(12,12.5,7,5.5,0.8); c.stroke()
        } else if (kind === "controller") {
            c.beginPath(); c.moveTo(6,8); c.lineTo(19,8); c.quadraticCurveTo(22,9,23,16);
            c.quadraticCurveTo(24,22,20,21); c.lineTo(16,18); c.lineTo(9,18); c.lineTo(5,21);
            c.quadraticCurveTo(1,22,2,16); c.quadraticCurveTo(3,9,6,8); c.stroke()
            line([6,13,11,13]); line([8.5,10.5,8.5,15.5]); circle(17,12,0.8); circle(20,15,0.8)
        } else if (kind === "mods") {
            line([3,8,12.5,3,22,8,12.5,13,3,8]); line([3,13,12.5,18,22,13]); line([3,18,12.5,23,22,18])
        } else if (kind === "online") {
            circle(8,10,3); circle(18,9,2.5); c.beginPath(); c.arc(8,21,6,Math.PI,2*Math.PI); c.stroke();
            c.beginPath(); c.arc(18,19,4,Math.PI,2*Math.PI); c.stroke()
        } else if (kind === "achievements") {
            line([7,4,18,4,17,15,14,18,11,18,8,15,7,4]);
            line([7,7,3,7,4,13,8,15]); line([18,7,22,7,21,13,17,15]); line([12.5,18,12.5,21]); line([8,22,17,22])
        } else if (kind === "advanced") {
            circle(12.5,12.5,7); circle(12.5,12.5,2.5)
            for (let a=0; a<8; a++) { let t=a*Math.PI/4; line([12.5+8.5*Math.cos(t),12.5+8.5*Math.sin(t),12.5+11*Math.cos(t),12.5+11*Math.sin(t)]) }
        } else if (kind === "log") {
            box(4,2,17,21,1); line([7,8,18,8]); line([7,12,18,12]); line([7,16,16,16])
        } else if (kind === "cheats") {
            line([13,2,5,14,12,14,10,23,20,10,13,10,13,2])
        } else {
            circle(12.5,12.5,8)
        }
    }
}

typedef struct {
    double x, y;
    double dist; // Distance réelle au segment
} SnapPoint;

SnapPoint project_point_on_segment(double mx, double my, double xA, double yA, double xB, double yB) {
    SnapPoint result;
    
    double dx = xB - xA;
    double dy = yB - yA;
    double length_sq = dx * dx + dy * dy;

    if (length_sq == 0) { // A et B sont le même point
        result.x = xA;
        result.y = yA;
    } else {
        // t est la position relative du projeté sur la droite (AB)
        // t = [(M-A) . (B-A)] / |B-A|^2
        double t = ((mx - xA) * dx + (my - yA) * dy) / length_sq;

        // On "clamp" t entre 0 et 1 pour rester sur le segment
        if (t < 0.0) t = 0.0;
        else if (t > 1.0) t = 1.0;

        result.x = xA + t * dx;
        result.y = yA + t * dy;
    }

    // Calcul de la distance finale entre la souris et le point projeté
    double diffX = mx - result.x;
    double diffY = my - result.y;
    result.dist = sqrt(diffX * diffX + diffY * diffY);

    return result;
}
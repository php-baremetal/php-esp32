<?php

namespace App\Controller;

use Symfony\Bundle\FrameworkBundle\Controller\AbstractController;
use Symfony\Component\HttpFoundation\Response;
use Symfony\Component\Routing\Attribute\Route;

class HelloController extends AbstractController
{
    #[Route('/', name: 'home')]
    public function home(): Response
    {
        $php  = PHP_VERSION;
        $sf   = \Symfony\Component\HttpKernel\Kernel::VERSION;
        $html = <<<HTML
        <!doctype html><html><head><meta charset="utf-8"><title>Symfony on ESP32</title>
        <style>body{font-family:system-ui,sans-serif;max-width:40rem;margin:4rem auto;padding:0 1rem;color:#1a1a1a}
        code{background:#f4f4f5;padding:.1rem .3rem;border-radius:.2rem}</style></head>
        <body><h1>Symfony is running on an ESP32</h1>
        <p>This page is served by the <strong>Symfony</strong> framework running on a microcontroller,
        one HTTP request at a time.</p>
        <ul><li>Symfony <code>$sf</code></li><li>PHP <code>$php</code></li></ul>
        <p>Rendered by a controller behind the firmware's <code>web-server</code> model.</p>
        </body></html>
        HTML;
        return new Response($html);
    }
}

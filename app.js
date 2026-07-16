document.getElementById("year").textContent = `© ${new Date().getFullYear()} ESP32 MQTT Energy Meter`;

const carousel = document.querySelector("[data-carousel]");

if (carousel) {
  const cards = Array.from(carousel.querySelectorAll(".carousel-card"));
  const dotsContainer = carousel.querySelector("[data-carousel-dots]");
  const prevButton = carousel.querySelector("[data-carousel-prev]");
  const nextButton = carousel.querySelector("[data-carousel-next]");
  let index = 0;
  let timer = null;

  const renderDots = () => {
    dotsContainer.innerHTML = "";
    cards.forEach((_, dotIndex) => {
      const dot = document.createElement("button");
      dot.type = "button";
      dot.className = "carousel-dot";
      dot.setAttribute("aria-label", `切換到第 ${dotIndex + 1} 張`);
      dot.addEventListener("click", () => {
        index = dotIndex;
        update();
        restart();
      });
      dotsContainer.appendChild(dot);
    });
  };

  const update = () => {
    cards.forEach((card, cardIndex) => {
      card.classList.toggle("is-active", cardIndex === index);
    });

    Array.from(dotsContainer.children).forEach((dot, dotIndex) => {
      dot.classList.toggle("is-active", dotIndex === index);
    });
  };

  const next = () => {
    index = (index + 1) % cards.length;
    update();
  };

  const prev = () => {
    index = (index - 1 + cards.length) % cards.length;
    update();
  };

  const restart = () => {
    window.clearInterval(timer);
    timer = window.setInterval(next, 4500);
  };

  renderDots();
  update();

  prevButton?.addEventListener("click", () => {
    prev();
    restart();
  });

  nextButton?.addEventListener("click", () => {
    next();
    restart();
  });

  restart();
}
